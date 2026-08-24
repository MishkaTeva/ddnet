#ifndef MOD_ENGINE_SERVER_MOD_SERVER_INTEGRATION_H
#define MOD_ENGINE_SERVER_MOD_SERVER_INTEGRATION_H

class CServer;

void ModServerInit(CServer *pServer);
void ModServerShutdown(CServer *pServer);
void ModServerTick(CServer *pServer);

#endif
