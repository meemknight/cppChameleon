#pragma once
#include <cstdint>
#include <enet/enet.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#define SERVER_CHANNELS 3

#define CHANNEL_CONNECTIONS 0
#define CHANNEL_GAMEPLAY 1
#define CHANNEL_PAINTING 2


struct Packet
{
	std::uint64_t cid = 0;
	uint32_t header = 0;
	char *getData()
	{
		return (char *)((&cid) + 1);
	}

	bool isCompressed() { return header & 0x8000'0000; }

	void setCompressed() { header |= 0x8000'0000; }

	void setNotCompressed() { header &= 0x7FFF'FFFF; }

};


enum: std::uint32_t
{
	headerNone = 0,
	headerReceiveCIDAndData,
	headerPlayerStateUpdate,
	headerPlayerPaintTextureUpdate,
	headerClientDisconnected,

};


struct Packet_ReceiveCIDAndData
{
	std::uint64_t yourCID = 0;
};

struct Packet_PlayerStateUpdate
{
	glm::vec3 position = {};
	float yaw = 0.0f;
	std::int32_t animationIndex = 0;
};

struct Packet_ClientDisconnected
{
	std::uint64_t cid = 0;
};

struct Packet_PlayerPaintTextureUpdate
{
	std::int32_t meshIndex = -1;
	glm::ivec2 size = {};
	std::int32_t quality = 0;
	std::uint32_t pixelDataSize = 0;
};

void *unCompressData(const char *data, size_t compressedSize, size_t &originalSize);

void sendPacketAndCompress(ENetPeer *to, Packet p,
	const char *data, size_t size, bool reliable, int channel);

void sendPacket(ENetPeer *to, Packet p, const char *data, size_t size, bool reliable, int channel);

//ton't use in client code!!
void sendPacket(ENetPeer *to, uint32_t header, void *data, size_t size, bool reliable, int channel);

void sendPacket(ENetPeer *to, uint32_t header, std::uint64_t cid, void *data, size_t size, bool reliable, int channel);


char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize);

char *parsePacket(ENetPacket &packet, Packet &p, size_t &dataSize);

void *compressData(const char *data, size_t size, size_t &compressedSize);

