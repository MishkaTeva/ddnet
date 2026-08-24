#ifndef GAME_SERVER_VERSIONCHECK_H
#define GAME_SERVER_VERSIONCHECK_H

class CVersionCheck
{
public:
	static bool IsVersionAllowed(int ClientVersion, int MinVersion);
	static const char* GetKickMessage();
};

#endif // GAME_SERVER_VERSIONCHECK_H
