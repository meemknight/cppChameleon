#include <ServerGameplay.h>
#include <iostream>


void ServerGameplay::init()
{

	//start enet server
	ENetAddress adress;
	adress.host = ENET_HOST_ANY;
	adress.port = 7771;
	ENetEvent event;

	//first param adress, players limit, channels, bandwith limit
	server = enet_host_create(&adress, 32, SERVER_CHANNELS, 0, 0);


	if (!server)
	{
		//we can't open the server
		exit(0);
	}

}

void ServerGameplay::update()
{

#pragma region host service

	int waitTime = 1;
	int tries = 10;

	ENetEvent event = {};

	while ((enet_host_service(server, &event, waitTime) > 0) || (waitTime = 0, tries-- > 0))
	{
		//we wait only the first time, than we want to let the server update happen.
		waitTime = 0;

		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		{
			addConnection(event);

			std::cout << "Successfully connected!\n";

			break;
		}
		case ENET_EVENT_TYPE_RECEIVE:
		{
			recieveDataFromClients(event);

			enet_packet_destroy(event.packet);

			break;
		}
		case ENET_EVENT_TYPE_DISCONNECT:
		{

			std::cout << "disconnect from server: "
				<< event.peer->address.host << " "
				<< event.peer->address.port << "\n\n";
			removeConnection(event);
			break;
		}


		}
	}


#pragma endregion






}

void ServerGameplay::close()
{

	//todo notify clients we closed

}

std::uint64_t ServerGameplay::getIdAndIncrement()
{
	return playerIDs++;
}


void ServerGameplay::addConnection(ENetEvent &event)
{

	event.peer->timeoutMinimum = 10'000;
	event.peer->timeoutMaximum = 30'000;
	event.peer->timeoutLimit = 64;

	std::uint64_t id = getIdAndIncrement();

	//send player data
	Packet p;
	p.header = headerReceiveCIDAndData;
	p.cid = id;

	Packet_ReceiveCIDAndData packetToSend = {};
	packetToSend.yourCID = id;

	//send own cid
	sendPacket(event.peer, p, (const char *)&packetToSend,
		sizeof(packetToSend), true, CHANNEL_CONNECTIONS);


	//todo send to others the fact that now we have a new connection


}

void ServerGameplay::recieveDataFromClients(ENetEvent &event)
{

}

void ServerGameplay::removeConnection(ENetEvent &event)
{

}
