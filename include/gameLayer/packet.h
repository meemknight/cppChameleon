#pragma once
#include <cstdint>
#include <enet/enet.h>

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

