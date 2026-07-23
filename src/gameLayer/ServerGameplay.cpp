#include <ServerGameplay.h>
#include <iostream>

namespace
{
	std::uint64_t getPeerCid(ENetPeer *peer)
	{
		return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(peer->data));
	}
}

bool ServerGameplay::init()
{
	close();
	playerIDs = 1;
	connectedClients = 0;

	//start enet server
	ENetAddress adress;
	adress.host = ENET_HOST_ANY;
	adress.port = 7769;
	ENetEvent event;

	//first param adress, players limit, channels, bandwith limit
	server = enet_host_create(&adress, 32, SERVER_CHANNELS, 0, 0);


	if (!server)
	{
		return false;
	}

	return true;

}

void ServerGameplay::update()
{
	if (!server)
	{
		return;
	}

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
	if (server)
	{
		enet_host_destroy(server);
		server = nullptr;
	}

	connectedClients = 0;

}

std::uint64_t ServerGameplay::getIdAndIncrement()
{
	return playerIDs++;
}


void ServerGameplay::addConnection(ENetEvent &event)
{
	connectedClients++;

	event.peer->timeoutMinimum = 10'000;
	event.peer->timeoutMaximum = 30'000;
	event.peer->timeoutLimit = 64;

	std::uint64_t id = getIdAndIncrement();
	event.peer->data = reinterpret_cast<void *>(static_cast<std::uintptr_t>(id));

	//send player data
	Packet p;
	p.header = headerReceiveCIDAndData;
	p.cid = id;

	Packet_ReceiveCIDAndData packetToSend = {};
	packetToSend.yourCID = id;

	//send own cid
	sendPacket(event.peer, p, (const char *)&packetToSend,
		sizeof(packetToSend), true, CHANNEL_CONNECTIONS);
	enet_host_flush(server);


	//todo send to others the fact that now we have a new connection


}

void ServerGameplay::recieveDataFromClients(ENetEvent &event)
{
	Packet packet = {};
	size_t payloadSize = 0;
	char *payload = parsePacket(event, packet, payloadSize);
	if (!payload)
	{
		return;
	}

	const std::uint32_t header = packet.header & 0x7FFF'FFFFu;
	const std::uint64_t cid = getPeerCid(event.peer);
	const bool reliable = (event.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0;

	switch (header)
	{
	case headerPlayerStateUpdate:
	{
		if (payloadSize >= sizeof(Packet_PlayerStateUpdate) && cid != 0)
		{
			packet.header = headerPlayerStateUpdate;
			packet.cid = cid;
			broadcastPacketToOtherClients(event.peer, packet, payload, sizeof(Packet_PlayerStateUpdate), reliable, CHANNEL_GAMEPLAY);
		}
		break;
	}

	case headerPlayerPaintTextureUpdate:
	{
		if (payloadSize >= sizeof(Packet_PlayerPaintTextureUpdate) && cid != 0)
		{
			packet.cid = cid;
			broadcastPacketToOtherClients(event.peer, packet, payload, payloadSize, true, CHANNEL_PAINTING);
		}
		break;
	}

	default:
		break;
	}

}

void ServerGameplay::removeConnection(ENetEvent &event)
{
	const std::uint64_t cid = getPeerCid(event.peer);
	event.peer->data = nullptr;

	if (connectedClients > 0)
	{
		connectedClients--;
	}

	if (cid != 0)
	{
		Packet packet = {};
		packet.header = headerClientDisconnected;
		packet.cid = cid;

		Packet_ClientDisconnected disconnectedPacket = {};
		disconnectedPacket.cid = cid;

		broadcastPacketToOtherClients(
			event.peer,
			packet,
			(const char *)&disconnectedPacket,
			sizeof(disconnectedPacket),
			true,
			CHANNEL_CONNECTIONS);
	}

}

void ServerGameplay::broadcastPacketToOtherClients(ENetPeer *sourcePeer, Packet packet,
	const char *data, size_t size, bool reliable, int channel)
{
	if (!server)
	{
		return;
	}

	for (size_t i = 0; i < server->peerCount; ++i)
	{
		ENetPeer &peer = server->peers[i];
		if (&peer == sourcePeer || peer.state != ENET_PEER_STATE_CONNECTED)
		{
			continue;
		}

		sendPacket(&peer, packet, data, size, reliable, channel);
	}
}
