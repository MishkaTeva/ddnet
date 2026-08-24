#include "discord_bridge.h"

#include <base/dbg.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/thread.h>
#include <base/time.h>

#include <chrono>
#include <thread>

#include <engine/shared/network.h>
#include <game/server/gamecontext.h>
#if defined(CONF_FAMILY_WINDOWS)
#include <stdlib.h>
#endif
#if !defined(CONF_FAMILY_WINDOWS)
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#endif

static const char *SkipJsonSpaces(const char *p)
{
	while(p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
		p++;
	return p;
}

static int UnescapeJsonString(const char *pIn, int InLen, char *pOut, int OutSize)
{
	// Converts common JSON escapes (\n, \r, \t, \\, \") into real chars.
	// Does not implement full \uXXXX (not needed here because ensure_ascii=False).
	int OutLen = 0;
	for(int i = 0; i < InLen && OutLen < OutSize - 1; i++)
	{
		char c = pIn[i];
		if(c == '\\' && i + 1 < InLen)
		{
			char n = pIn[i + 1];
			i++;
			switch(n)
			{
			case 'n': pOut[OutLen++] = '\n'; break;
			case 'r': pOut[OutLen++] = '\r'; break;
			case 't': pOut[OutLen++] = '\t'; break;
			case '\\': pOut[OutLen++] = '\\'; break;
			case '"': pOut[OutLen++] = '"'; break;
			case '/': pOut[OutLen++] = '/'; break;
			default:
				// keep unknown escape as-is (drop backslash)
				pOut[OutLen++] = n;
				break;
			}
		}
		else
		{
			pOut[OutLen++] = c;
		}
	}
	pOut[OutLen] = 0;
	return OutLen;
}

static bool ExtractJsonStringField(const char *pJson, const char *pField, char *pOut, int OutSize)
{
	pOut[0] = 0;
	if(!pJson || !pField || !pField[0] || OutSize <= 1)
		return false;

	// Match JSON object keys only ("field": value), not string values like "event": "message".
	char aNeedle[128];
	str_format(aNeedle, sizeof(aNeedle), "\"%s\":", pField);
	const char *pKey = str_find(pJson, aNeedle);
	if(!pKey)
		return false;

	const char *p = pKey + str_length(aNeedle);
	p = SkipJsonSpaces(p);
	if(!p || *p != '"')
		return false;
	p++; // after opening quote

	// find closing quote that isn't escaped
	const char *pStart = p;
	bool Escaped = false;
	for(; *p; p++)
	{
		if(Escaped)
		{
			Escaped = false;
			continue;
		}
		if(*p == '\\')
		{
			Escaped = true;
			continue;
		}
		if(*p == '"')
			break;
	}
	if(*p != '"')
		return false;

	int RawLen = (int)(p - pStart);
	UnescapeJsonString(pStart, RawLen, pOut, OutSize);
	return true;
}

CDiscordBridge::CDiscordBridge()
{
	m_pGameServer = nullptr;
	m_Socket = 0;
	m_Port = 0;
	m_Running = false;
	m_pThread = nullptr;
}

CDiscordBridge::~CDiscordBridge()
{
	Stop();
}

bool CDiscordBridge::Start(int Port, IGameServer *pGameServer)
{
	if(m_Running)
		return false;
	
	m_pGameServer = pGameServer;
	m_Port = Port;
	
	// Создаем TCP сокет
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_IPV4;
	// Привязываем HTTP‑сервер только к 127.0.0.1, чтобы порт не торчал в интернет.
	// NETADDR_SIZE_IPV4 == 4, первые 4 байта ip используются для IPv4.
	BindAddr.ip[0] = 127;
	BindAddr.ip[1] = 0;
	BindAddr.ip[2] = 0;
	BindAddr.ip[3] = 1;
	BindAddr.port = Port;
	
	m_Socket = net_tcp_create(BindAddr);
	if(!m_Socket)
	{
		dbg_msg("discord_bridge", "Failed to create socket on port %d", Port);
		return false;
	}
	
	net_set_non_blocking(m_Socket);
	
	if(net_tcp_listen(m_Socket, 5) != 0)
	{
		dbg_msg("discord_bridge", "Failed to listen on port %d — port already in use, killing old bot process", Port);
		net_tcp_close(m_Socket);
		m_Socket = 0;

#if !defined(CONF_FAMILY_WINDOWS)
		// Убиваем старый Python бот который держит порт
		int r = system("pkill -f discord_bot.py 2>/dev/null");
		(void)r;
		// Ждём до 5 секунд пока порт освободится
		for(int Retry = 0; Retry < 5; Retry++)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			m_Socket = net_tcp_create(BindAddr);
			if(!m_Socket) continue;
			net_set_non_blocking(m_Socket);
			if(net_tcp_listen(m_Socket, 5) == 0)
			{
				dbg_msg("discord_bridge", "Successfully bound port %d after %d second(s)", Port, Retry + 1);
				goto bound_ok;
			}
			net_tcp_close(m_Socket);
			m_Socket = 0;
		}
		dbg_msg("discord_bridge", "Still failed to listen on port %d after retries", Port);
		return false;
		bound_ok:;
#else
		return false;
#endif
	}
	
	m_Running = true;
	m_pThread = thread_init(ThreadFunc, this, "discord_bridge");
	
	dbg_msg("discord_bridge", "Started on port %d", Port);
	
	// Запускаем Discord бота автоматически
#if defined(CONF_FAMILY_WINDOWS)
	{
		int r = system("powershell -NoProfile -Command \"Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*discord_bot.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }\" 2>nul");
		(void)r;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	int result = system("start /MIN cmd /c \"python discord_bot.py > discord_bot.log 2>&1\"");
	(void)result;
	dbg_msg("discord_bridge", "Discord bot started (check discord_bot.log for output)");
#else
	// Убиваем старый бот если есть, ждём завершения
	{
		int r = system("pkill -f discord_bot.py 2>/dev/null");
		(void)r;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	// Используем fork чтобы дочерний процесс не наследовал файловые дескрипторы сервера
	{
		pid_t pid = fork();
		if(pid == 0)
		{
			int maxfd = (int)sysconf(_SC_OPEN_MAX);
			for(int fd = 3; fd < maxfd; fd++)
				close(fd);
			setsid();
			int logfd = open("discord_bot.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(logfd >= 0) { dup2(logfd, STDOUT_FILENO); dup2(logfd, STDERR_FILENO); close(logfd); }
			execlp("python3", "python3", "discord_bot.py", (char*)nullptr);
			_exit(1);
		}
	}
	dbg_msg("discord_bridge", "Discord bot started (check discord_bot.log for output)");
#endif
	
	return true;
}

void CDiscordBridge::Stop()
{
	if(!m_Running)
		return;
	
	m_Running = false;
	
	if(m_Socket)
	{
		net_tcp_close(m_Socket);
		m_Socket = 0;
	}
	
	if(m_pThread)
	{
		thread_wait(m_pThread);
		m_pThread = nullptr;
	}
	
	dbg_msg("discord_bridge", "Stopped");
}

void CDiscordBridge::ThreadFunc(void *pUser)
{
	((CDiscordBridge *)pUser)->Run();
}

void CDiscordBridge::Run()
{
	while(m_Running)
	{
		NETSOCKET ClientSocket = 0;
		NETADDR ClientAddr;
		mem_zero(&ClientAddr, sizeof(ClientAddr));
		
		int Result = net_tcp_accept(m_Socket, &ClientSocket, &ClientAddr);
		
		if(Result > 0 && ClientSocket)
		{
			net_set_blocking(ClientSocket);
			HandleRequest(ClientSocket);
			net_tcp_close(ClientSocket);
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void CDiscordBridge::HandleRequest(NETSOCKET Socket)
{
	char aBuffer[4096];
	mem_zero(aBuffer, sizeof(aBuffer));

	// Читаем весь запрос целиком: заголовки + тело. Один вызов net_tcp_recv()
	// может вернуть только часть данных (например, заголовки в одном TCP-сегменте,
	// а тело — в следующем), поэтому читаем в цикле, пока не придёт всё тело
	// согласно Content-Length. Иначе тело теряется и JSON оказывается пустым.
	int TotalReceived = 0;
	while(TotalReceived < (int)sizeof(aBuffer) - 1)
	{
		int Received = net_tcp_recv(Socket, (unsigned char *)aBuffer + TotalReceived, (sizeof(aBuffer) - 1) - TotalReceived);
		if(Received <= 0)
			break;
		TotalReceived += Received;
		aBuffer[TotalReceived] = 0;

		const char *pHeaderEnd = str_find(aBuffer, "\r\n\r\n");
		if(pHeaderEnd)
		{
			int HeaderLen = (int)(pHeaderEnd - aBuffer) + 4;
			int ContentLength = 0;
			const char *pCL = str_find(aBuffer, "Content-Length:");
			if(pCL)
				ContentLength = atoi(pCL + str_length("Content-Length:"));
			if(TotalReceived >= HeaderLen + ContentLength)
				break;
		}
	}

	dbg_msg("discord_bridge", "Received %d bytes", TotalReceived);

	if(TotalReceived <= 0)
	{
		const char *pResponse = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
		net_tcp_send(Socket, pResponse, str_length(pResponse));
		return;
	}

	aBuffer[TotalReceived] = 0;

	// Проверяем POST /discord
	if(str_find(aBuffer, "POST /discord") == nullptr)
	{
		dbg_msg("discord_bridge", "Not a POST /discord request");
		const char *pResponse = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
		net_tcp_send(Socket, pResponse, str_length(pResponse));
		return;
	}
	
	// Ищем тело запроса
	const char *pBody = str_find(aBuffer, "\r\n\r\n");
	if(!pBody)
	{
		dbg_msg("discord_bridge", "No body found");
		const char *pResponse = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
		net_tcp_send(Socket, pResponse, str_length(pResponse));
		return;
	}
	
	pBody += 4;
	
	// Парсим JSON
	char aUsername[64] = "Discord";
	char aMessage[2048] = "";
	char aAvatarURL[256] = "";
	char aUserID[32] = "";
	char aReplyTo[64] = "";
	char aEvent[32] = "message";

	// Минимальный корректный парсер строковых полей JSON (с обработкой \\n/\\r/\\t/\\\"/\\\\).
	ExtractJsonStringField(pBody, "username", aUsername, sizeof(aUsername));
	ExtractJsonStringField(pBody, "message", aMessage, sizeof(aMessage));
	ExtractJsonStringField(pBody, "avatar_url", aAvatarURL, sizeof(aAvatarURL));
	ExtractJsonStringField(pBody, "user_id", aUserID, sizeof(aUserID));
	ExtractJsonStringField(pBody, "reply_to", aReplyTo, sizeof(aReplyTo));
	ExtractJsonStringField(pBody, "event", aEvent, sizeof(aEvent));
	
	// Преобразуем URL в короткие теги (http/https/www/discord.gg)
	char aProcessedMessage[2048];
	int OutPos = 0;
	const char *pRead = aMessage;
	while(*pRead && OutPos < (int)sizeof(aProcessedMessage) - 12)
	{
		const bool IsHttp = str_comp_num(pRead, "http://", 7) == 0 || str_comp_num(pRead, "https://", 8) == 0;
		const bool IsWww = str_comp_num(pRead, "www.", 4) == 0;
		const bool IsDiscordInvite = str_comp_num(pRead, "discord.gg/", 11) == 0 || str_comp_num(pRead, "discord.com/invite/", 19) == 0;
		if(IsHttp || IsWww || IsDiscordInvite)
		{
			while(*pRead && *pRead != ' ' && *pRead != '\n' && *pRead != '\r' && *pRead != '\t')
				pRead++;

			const char *pTag = IsDiscordInvite ? "[invite]" : "[link]";
			int TagLen = str_length(pTag);
			str_copy(aProcessedMessage + OutPos, pTag, sizeof(aProcessedMessage) - OutPos);
			OutPos += TagLen;
		}
		else
		{
			aProcessedMessage[OutPos++] = *pRead++;
		}
	}
	aProcessedMessage[OutPos] = 0;

	// Сжать повторные пробелы после замены ссылок
	{
		int W = 0;
		bool PrevSpace = false;
		for(int R = 0; aProcessedMessage[R]; R++)
		{
			char c = aProcessedMessage[R];
			if(c == ' ')
			{
				if(PrevSpace)
					continue;
				PrevSpace = true;
			}
			else
				PrevSpace = false;
			aProcessedMessage[W++] = c;
		}
		while(W > 0 && aProcessedMessage[W - 1] == ' ')
			W--;
		aProcessedMessage[W] = 0;
	}
	str_copy(aMessage, aProcessedMessage, sizeof(aMessage));

	// Ответы: "↪ Alice: text" (реакции уже полностью собраны в Python)
	if(aReplyTo[0] && str_comp(aEvent, "reaction") != 0)
	{
		// Обрезаем суффикс " [число]" / " [-число]" из ника игрока (webhook)
		int i = str_length(aReplyTo) - 1;
		if(i >= 0 && aReplyTo[i] == ']')
		{
			i--;
			while(i > 0 && ((aReplyTo[i] >= '0' && aReplyTo[i] <= '9') || aReplyTo[i] == '-'))
				i--;
			if(i > 0 && aReplyTo[i] == '[')
				aReplyTo[(i > 1 && aReplyTo[i-2] == ' ') ? i-2 : i-1] = 0;
		}
		char aTmp[2048];
		str_format(aTmp, sizeof(aTmp), "\xE2\x86\xAA %s: %s", aReplyTo, aMessage); // ↪
		str_copy(aMessage, aTmp, sizeof(aMessage));
	}
	
	dbg_msg("discord_bridge", "Parsed event=%s username='%s' message='%s'", aEvent, aUsername, aMessage);
	
	if(aMessage[0] && m_pGameServer)
	{
		CGameContext *pGameServer = (CGameContext *)m_pGameServer;
		// Внутриигровой чат однострочный — переводим управляющие символы в пробелы.
		char aChatMsg[2048];
		int Out = 0;
		for(const char *p = aMessage; *p && Out < (int)sizeof(aChatMsg) - 1; p++)
		{
			char c = *p;
			if(c == '\n' || c == '\r' || c == '\t')
				c = ' ';
			aChatMsg[Out++] = c;
		}
		aChatMsg[Out] = 0;

		// Drop duplicate events (e.g. two bot processes posting the same payload).
		{
			static char s_aLastDedup[512];
			static int64_t s_LastDedupTime = 0;
			char aDedup[512];
			str_format(aDedup, sizeof(aDedup), "%s|%s|%s", aUserID, aEvent, aChatMsg);
			const int64_t Now = time_get();
			if(str_comp(s_aLastDedup, aDedup) == 0 && Now - s_LastDedupTime < time_freq())
			{
				const char *pResponse = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
				net_tcp_send(Socket, pResponse, str_length(pResponse));
				return;
			}
			str_copy(s_aLastDedup, aDedup, sizeof(s_aLastDedup));
			s_LastDedupTime = Now;
		}

		pGameServer->HandleDiscordMessage(aUsername, aUserID, aChatMsg, aEvent);
	}
	else
	{
		dbg_msg("discord_bridge", "Message empty or no game server");
	}
	
	const char *pResponse = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
	net_tcp_send(Socket, pResponse, str_length(pResponse));
}
