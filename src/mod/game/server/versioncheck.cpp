#include "versioncheck.h"

#include <engine/shared/protocol.h>

bool CVersionCheck::IsVersionAllowed(int ClientVersion, int MinVersion)
{
	if(MinVersion == 0)
		return true;

	if(ClientVersion == 0)
		return true;

	// Placeholder from crashmeplx until Cl_IsDDNet arrives (AllTheHaxx etc.)
	if(ClientVersion <= 1)
		return true;

	// Legacy clients below modern DDNet — allow (AllTheHaxx=10072, etc.)
	if(ClientVersion < VERSION_DDNET_MSG_LEGACY)
		return true;

	if(ClientVersion < MinVersion)
		return false;

	return true;
}

const char* CVersionCheck::GetKickMessage()
{
	return "Your client is not supported";
}
