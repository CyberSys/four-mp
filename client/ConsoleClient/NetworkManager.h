#pragma once

#include "NetworkIDObject.h"
#include "NetworkIDManager.h"
#include "../../Shared/RPC3/RPC3.h"
#include "RakPeerInterface.h"

#include "../../Shared/Network/NetworkProtocol.h"

class NetworkManager : public NetworkIDObject
{
public:
	NetworkManager(void);
	~NetworkManager(void);
	void Load(void);
	void Tick(void);
	void Unload(void);
	void Ping(const char *hostname, const unsigned short port = 7777);
	void ConnectToServer(const char *hostname = "", const unsigned short port = 7777);
	void SendClientConnectionRequest(void);
	void SendPlayerMove(const float speed);
	void SendPlayerJump(void);
	void SendPlayerDuck(void);
	void SendPlayerEntranceInVehicle(const char seat);
	void SendPlayerCancelEntranceInVehicle(void);
	void SendPlayerExitFromVehicle(void);
	void SendPlayerFire(const float position[3], const int time, const short target, const unsigned int health, const unsigned int armor);
	void SendPlayerAim(const float position[3], const int time);
	void SendPlayerWeaponChange(void);
	void SendPlayerHealthAndArmorChange(void);
	void SendPlayerSpawnRequest(void);
	void SendPlayerModelChange(void);
	void SendPlayerComponentsChange(void);
	void SendPlayerChat(void);
	void RecieveClientConnection(NetworkPlayerFullUpdateData data, RakNet::RPC3 *serverrpc3);
	void RecieveClientConnectionError(NetworkPlayerConnectionErrorData data, RakNet::RPC3 *serverrpc3);
	void RecieveClientDisconnection(NetworkPlayerDisconnectionData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerFullUpdate(NetworkPlayerFullUpdateData data, RakNet::RPC3 *serverrpc3);
	void RecieveVehicleFullUpdate(NetworkVehicleFullUpdateData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerMove(NetworkPlayerMoveData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerJump(NetworkPlayerJumpData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerDuck(NetworkPlayerDuckData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerEntranceInVehicle(NetworkPlayerEntranceInVehicleData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerCancelEntranceInVehicle(NetworkPlayerCancelEntranceInVehicleData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerExitFromVehicle(NetworkPlayerExitFromVehicleData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerFire(NetworkPlayerFireData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerAim(NetworkPlayerAimData, RakNet::RPC3 *serverrpc3);
	void RecievePlayerWeaponChange(NetworkPlayerWeaponChangeData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerHealthAndArmorChange(NetworkPlayerHealthAndArmorChangeData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerSpawn(NetworkPlayerSpawnData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerModelChange(NetworkPlayerModelChangeData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerComponentsChange(NetworkPlayerComponentsChangeData data, RakNet::RPC3 *serverrpc3);
	void RecievePlayerChat(NetworkPlayerChatData data, RakNet::RPC3 *serverrpc3);
	void RecieveNewVehicle(NetworkVehicleFullUpdateData data, RakNet::RPC3 *serverrpc3);
private:
	NetworkIDManager *manager;
	NetworkID clientid;
	NetworkID serverid;
	SystemAddress serveraddress;
	RakNet::RPC3 *rpc3;
	RakPeerInterface *net;
};