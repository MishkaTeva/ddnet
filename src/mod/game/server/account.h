// Slim F-DDrace account layer for DDNet-base migration (Phase 5).
// File format is intentionally small; full .acc parity comes later.

#ifndef MOD_GAME_SERVER_ACCOUNT_H
#define MOD_GAME_SERVER_ACCOUNT_H

#include <base/hash.h>
#include <base/types.h>

#include <engine/shared/protocol.h>

#include <vector>

class CGameContext;
class CPlayer;

enum
{
	ACC_START = 1,
	VIP_NONE = 0,
	VIP_CLASSIC = 1,
	VIP_PLUS = 2,
	VIP_ADMIN = 5,
	ACC_USERNAME_LENGTH = 32,
};

struct SAccountInfo
{
	SHA256_DIGEST m_Password{};
	char m_aUsername[ACC_USERNAME_LENGTH]{};
	int m_ClientId = -1;
	bool m_LoggedIn = false;
	bool m_Disabled = false;
	int m_VIP = VIP_NONE;
	int64_t m_ExpireDateVIP = 0;
};

class CAccountSystem
{
	CGameContext *m_pGameServer = nullptr;
	std::vector<SAccountInfo> m_vAccounts;

	void EnsureDir() const;
	void AccountPath(char *pBuf, int BufSize, const char *pUsername) const;
	bool ReadFile(int AccId);
	bool WriteFile(int AccId) const;
	static SHA256_DIGEST HashPassword(const char *pPassword);
	int AllocSlot();

public:
	void Init(CGameContext *pGameServer);
	void Shutdown();

	SAccountInfo *Get(int AccId);
	const SAccountInfo *Get(int AccId) const;
	int Num() const { return (int)m_vAccounts.size(); }

	int FindByUsername(const char *pUsername) const;
	int FindByClientId(int ClientId) const;

	bool Register(int ClientId, const char *pUsername, const char *pPassword);
	bool Login(int ClientId, const char *pUsername, const char *pPassword);
	void Logout(int AccId, bool Silent = false);
	void LogoutClient(int ClientId, bool Silent = false);

	void SetVip(int AccId, int VipLevel, int64_t ExpireDate = 0);
	void SyncMoveRestrictions(CPlayer *pPlayer) const;

	void JailPlayer(int ClientId, int Seconds);
	void ReleaseJail(int ClientId);
	bool IsJailed(int ClientId) const;
};

#endif
