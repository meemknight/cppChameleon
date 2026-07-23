#pragma once
#include <packet.h>



struct ServerGameplay
{

	ENetHost *server = nullptr;

	bool init();

	void update();

	void close();

	void addConnection(ENetEvent& event);
	
	void recieveDataFromClients(ENetEvent& event);
	
	void removeConnection(ENetEvent& event);

	void broadcastPacketToOtherClients(ENetPeer *sourcePeer, Packet packet,
		const char *data, size_t size, bool reliable, int channel);

	std::uint64_t getIdAndIncrement();

	std::uint64_t playerIDs = 1;
	std::uint32_t connectedClients = 0;
};
