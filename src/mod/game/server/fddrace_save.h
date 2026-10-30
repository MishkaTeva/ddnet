#ifndef MOD_GAME_SERVER_FDDRACE_SAVE_H
#define MOD_GAME_SERVER_FDDRACE_SAVE_H

#include <engine/shared/protocol.h>

#include <ctime>
#include <string>
#include <vector>

class CGameContext;

struct SSavedIdentity
{
	char m_aAccUsername[32]{};
	NETADDR m_Addr{};
	char m_aTimeoutCode[64]{};
	char m_aName[MAX_NAME_LENGTH]{};
	time_t m_ExpireDate = 0;
	int m_RedirectTilePort = 0;
	bool m_IsPeriodic = false;
	bool m_IsJailSave = false;
	bool m_IsMain = true;
	bool m_IsRedirectSave = false;
	int64_t m_JailSeconds = 0;
	int64_t m_WalletMoney = 0;
	int m_CarriedFlag = -1;
	char m_aFileHash[65]{};
};

enum
{
	SAVE_WALLET = 1 << 0,
	SAVE_FLAG = 1 << 1,
	SAVE_JAIL = 1 << 3,
	SAVE_REDIRECT = 1 << 4, // write under x_redirect_tile for cross-server load
	SAVE_PERIODIC = 1 << 5,
	SAVE_DDNET_DUMMY = 1 << 6,
};

class CFddraceSave
{
	CGameContext *m_pGameServer = nullptr;
	std::vector<SSavedIdentity> m_vSavedIdentities;
	std::vector<std::string> m_vSavedIdentitiesFiles;
	int64_t m_LastPeriodicSaveTick = 0;
	bool m_KeepSavedTeesOnDisconnect = false;

	static int LoadSavedPlayersCallback(const char *pName, int IsDir, int StorageType, void *pUser);

public:
	void Init(CGameContext *pGameServer);
	void OnMapLoaded();
	void OnTick();
	void OnShutdown();

	int SaveCharacter(int ClientId, int Flags, float Hours = -1.f);
	bool CheckLoadPlayer(int ClientId, bool FromSpawn = false);
	bool RemoveJailSave(int ClientId);
	void RemoveNonJailSaves(int ClientId);
	void SetKeepSavedTeesOnDisconnect(bool Keep) { m_KeepSavedTeesOnDisconnect = Keep; }
	bool KeepSavedTeesOnDisconnect() const { return m_KeepSavedTeesOnDisconnect; }
	void SaveDrop(int ClientId, float Hours, const char *pReason);
	void ListSavedTees() const;
	int NumSavedIdentities() const { return (int)m_vSavedIdentities.size(); }

private:
	void ReadSavedPlayersFile();
	void ExpireSavedIdentities();
	void RemoveSavedIdentityFile(const SSavedIdentity &Identity);
	void EnsureSaveDirs() const;

	int FindSavedPlayer(int ClientId) const;
	bool TryLoadPlayer(int ClientId, int Index);
	void RemoveDuplicateSaves(const SSavedIdentity &Info, int Flags);

	const char *GetSavedIdentityHash(const SSavedIdentity &Info) const;
	static const char *SavedIdentityFileStem(const SSavedIdentity &Info, const CFddraceSave *pSave);

	bool WriteSaveFile(const char *pPath, int Flags, const SSavedIdentity &Identity, class CCharacter *pChr);
	bool ReadSaveFile(const char *pPath, SSavedIdentity &Identity, int *pFlags, class CSaveTee *pSaveTee) const;
	void FormatIdentitySavePath(char *pBuf, int BufSize, const SSavedIdentity &Identity) const;

	bool IsLoggedIn(int ClientId) const;
	const char *AccUsername(int ClientId) const;
};

#endif
