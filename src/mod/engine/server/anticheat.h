#ifndef ENGINE_SERVER_ANTICHEAT_H
#define ENGINE_SERVER_ANTICHEAT_H

#include <engine/shared/protocol.h>
#include <base/types.h>
#include <vector>
#include <string>

class IServer;

// Fingerprint клиента — набор данных для идентификации
struct CClientFingerprint
{
	int m_DDNetVersion;           // числовой код версии (19080)
	char m_aVersionStr[64];       // строка версии ("TClient 10.8.7 (hash)")
	char m_aIamTaterStr[128];     // строка IAMTATER ("10.8.7 built on Mar 9 2026, ...")
	bool m_Sevendown;             // подключился через 0.7 протокол
	bool m_TClientVerified;       // прошёл checksum challenge
	bool m_HasHardcodedHash;      // хардкоженный хеш ddnet-custom
	bool m_SentIamTater;          // отправил IAMTATER

	CClientFingerprint()
	{
		m_DDNetVersion = 0;
		m_aVersionStr[0] = 0;
		m_aIamTaterStr[0] = 0;
		m_Sevendown = false;
		m_TClientVerified = false;
		m_HasHardcodedHash = false;
		m_SentIamTater = false;
	}
};

// Запись в whitelist/blacklist
struct CClientListEntry
{
	// Поля для матчинга (пустая строка = не проверять)
	int m_DDNetVersion;        // 0 = не проверять
	char m_aVersionStr[64];    // поддерживает * как wildcard
	char m_aIamTaterStr[128];  // поддерживает * как wildcard
	int m_Sevendown;           // -1 = не проверять, 0 = нет, 1 = да
	int m_TClientVerified;     // -1 = не проверять, 0 = нет, 1 = да
	int m_HasHardcodedHash;    // -1 = не проверять, 0 = нет, 1 = да

	char m_aComment[128];      // комментарий (имя игрока и т.д.)
	bool m_IsWhitelist;
	bool m_IsOldFormat;
	char m_aHashOnly[64];        // true = whitelist, false = blacklist

	CClientListEntry()
	{
		m_DDNetVersion = 0;
		m_aVersionStr[0] = 0;
		m_aIamTaterStr[0] = 0;
		m_Sevendown = -1;
		m_TClientVerified = -1;
		m_HasHardcodedHash = -1;
		m_aComment[0] = 0;
		m_IsWhitelist = true;
	m_IsOldFormat = false;
	m_aHashOnly[0] = 0;
	}
};

class CAntiCheat
{
public:
	CAntiCheat();

	void Init(IServer *pServer, class IStorage *pStorage);

	// Проверка клиента при получении информации о версии
	bool ValidateClient(int ClientID, char *pKickReason, int KickReasonSize);
	bool ValidateClientWithData(int ClientID, int DDNetVersion, const char *pDDNetVersionStr,
		char *pKickReason, int KickReasonSize);

	static const char* GetKickMessage();

	void OnClientMessage(int ClientID, int MsgID, bool System);
	void OnClientDisconnect(int ClientID);

	// Загрузка конфига
	void LoadConfig();

	// Проверка хеша клиента (старая система, для совместимости)
	bool ValidateClientHash(const char *pClientHash, int DDNetVersion);

	// Получить fingerprint игрока
	CClientFingerprint GetFingerprint(int ClientID) const;

	// Добавить игрока в whitelist по его fingerprint
	bool AddToWhitelist(int ClientID, const char *pComment);

	// Добавить игрока в blacklist по его fingerprint
	bool AddToBlacklist(int ClientID, const char *pComment);

	// Добавить готовую запись напрямую (для rcon add_whitelist/add_blacklist)
	void AddHashEntry(const CClientListEntry &Entry);

	// Удалить записи по хешу (для rcon remove_whitelist/remove_blacklist)
	// Возвращает количество удалённых записей
	int RemoveHashEntries(const char *pHashes, bool bWhitelist);

	// Сохранить конфиг
	void SaveConfig();

	// Вывести whitelist/blacklist в консоль
	void PrintWhitelist(class IConsole *pConsole) const;
	void PrintBlacklist(class IConsole *pConsole) const;

	// Тик
	void Tick();

	// Поместить нарушителя в тюрьму и отправить подробный отчёт модераторам.
	bool HandleViolation(int ClientID, const char *pReason);

private:
	IServer *m_pServer;
	class IStorage *m_pStorage;

	struct CClientData
	{
		int m_InfoMessages;
		int m_ReadyMessages;
		int m_EnterGameMessages;
		int m_StartInfoMessages;
		bool m_SentUuidMessage;
		bool m_HasConnectionID;
		bool m_SentClientVer;
		bool m_Checked;
		bool m_PendingJail; // wait for save load before jailing
		char m_aPendingJailReason[128];
		int64_t m_ConnectTime;
		int64_t m_FirstMessageTime;
		int64_t m_LastCheckTime;
		int m_SuspiciousScore;

		void Reset();
	};

	CClientData m_aClients[MAX_CLIENTS];

	std::vector<CClientListEntry> m_vWhitelist;
	std::vector<CClientListEntry> m_vBlacklist;

	// Старый whitelist хешей (для совместимости)
	std::vector<std::string> m_vWhitelistedHashes;

	bool AnalyzeClientBehavior(int ClientID);
	int GetMode() const;

public:
	bool CheckClientAllowed(int ClientID) const;

private:

	// Матчинг fingerprint с записью
	bool MatchFingerprint(const CClientFingerprint &FP, const CClientListEntry &Entry) const;

	// Wildcard матчинг строк (* = любая подстрока)
	static bool WildcardMatch(const char *pStr, const char *pPattern);

	// Сериализация fingerprint в строку для файла
	static void FingerprintToString(const CClientFingerprint &FP, char *pBuf, int BufSize);

	// Парсинг записи из строки файла
	static bool ParseEntry(const char *pLine, CClientListEntry &Entry);
};

#endif
