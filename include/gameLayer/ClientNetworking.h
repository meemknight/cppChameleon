#pragma once

#include <packet.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <map>
#include <string>
#include <vector>

struct ClientNetworking
{
	struct RemotePlayerState
	{
		glm::vec3 position = {};
		float yaw = 0.0f;
		std::int32_t animationIndex = 0;
	};

	struct RemotePaintTextureUpdate
	{
		int meshIndex = -1;
		glm::ivec2 size = {};
		int quality = 0;
		std::vector<unsigned char> pixels;
	};

	enum class ConnectionState
	{
		Disconnected,
		Connecting,
		Connected,
		Failed,
	};

	bool connectToServer(const char *serverAddress);
	bool sendPlayerState(const Packet_PlayerStateUpdate &playerState, bool reliable);
	bool sendPaintTextureUpdate(int meshIndex, glm::ivec2 size, int quality,
		const std::vector<unsigned char> &pixels);
	void update();
	void shutdown();

	void receiveDataFromServer(ENetEvent &event);

	const char *getConnectionStateName() const;

	ENetHost *client = nullptr;
	ENetPeer *serverPeer = nullptr;
	ConnectionState connectionState = ConnectionState::Disconnected;
	std::string connectedServerAddress = "localhost";
	std::string lastStatus = "Disconnected";
	std::uint64_t localCID = 0;
	bool receivedPlayerData = false;
	std::map<std::uint64_t, RemotePlayerState> remotePlayers;
	std::map<std::uint64_t, std::map<int, RemotePaintTextureUpdate>> remotePaintUpdates;
};
