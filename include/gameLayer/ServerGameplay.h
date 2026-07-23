#pragma once
#include <packet.h>



struct ServerGameplay
{

	ENetHost *server = nullptr;

	void init();

	void update();

	void close();

	void addConnection(ENetEvent& event);
	
	void recieveDataFromClients(ENetEvent& event);
	
	void removeConnection(ENetEvent& event);

	std::uint64_t getIdAndIncrement();

	std::uint64_t playerIDs = 1;
};