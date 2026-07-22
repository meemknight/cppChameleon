#include <packet.h>
#include <zstd.h>
#include <glm/vec2.hpp>
#include <utility>


void *compressData(const char *data, size_t size, size_t &compressedSize)
{

	size_t newExpectedMaxSize = size * 0.8;

	char *rezult = new char[newExpectedMaxSize];

	compressedSize = ZSTD_compress(rezult, newExpectedMaxSize, data, size, 3);


	if (ZSTD_isError(compressedSize))
	{
		delete[] rezult;
		return 0;
	}


	return rezult;

}



void *unCompressData(const char *data, size_t compressedSize, size_t &originalSize)
{
	// Decompress data
   // First, we need to get the decompressed size
	originalSize = ZSTD_getDecompressedSize(data, compressedSize);
	if (originalSize == ZSTD_CONTENTSIZE_ERROR || originalSize == 0)
	{
		// Failed to get decompressed size
		return nullptr;
	}

	char *decompressedData = new char[originalSize];
	if (!decompressedData)
	{
		// Memory allocation failed
		return nullptr;
	}

	// Decompress data
	size_t result = ZSTD_decompress(decompressedData, originalSize, data, compressedSize);
	if (ZSTD_isError(result))
	{
		// Decompression failed
		delete[] decompressedData;
		return nullptr;
	}

	return decompressedData;
}



void sendPacketAndCompress(ENetPeer *to,
	Packet p, const char *data, size_t size,
	bool reliable, int channel)
{
	size_t compressedSize = 0;
	char *compressedData = (char *)compressData(data, size, compressedSize);

	if (!compressedData)
	{
		sendPacket(to, p, data, size, reliable, channel);
	}
	else
	{
		if (compressedSize >= size)
		{
			sendPacket(to, p, data, size, reliable, channel);
		}
		else
		{

			//std::cout << "CHUNK SIZE BYTES: " << compressedSize << "\n";
			//std::cout << "CompressedSize: " << compressedSize << "\n";

			//std::cout << "compressed\n";
			p.setCompressed();
			sendPacket(to, p, compressedData, compressedSize, reliable, channel);
		}

		delete[] compressedData;

	}
}

void sendPacket(ENetPeer *to, Packet p,
	const char *data, size_t size, bool reliable, int channel)
{

	assert((data && size) || (!data && !size), "Assert failed in sendPacket");

	size_t flag = 0;

	//memcpy(data, &p, sizeof(Packet));

	if (reliable)
	{
		flag = ENET_PACKET_FLAG_RELIABLE;
	}
	else
	{
		flag = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
	}

	ENetPacket *packet = enet_packet_create(nullptr, size + sizeof(Packet), flag);

	memcpy(packet->data, &p, sizeof(Packet));
	memcpy(packet->data + sizeof(Packet), data, size);

	enet_peer_send(to, channel, packet);
}

//ton't use in client code!!
void sendPacket(ENetPeer *to, uint32_t header, void *data, size_t size, bool reliable, int channel)
{
	Packet packet;
	packet.header = header;

	sendPacket(to, packet, (const char *)data, size, reliable, channel);
}

void sendPacket(ENetPeer *to, uint32_t header, std::uint64_t cid, void *data, size_t size, bool reliable, int channel)
{
	Packet packet;
	packet.header = header;
	packet.cid = cid;

	sendPacket(to, packet, (const char *)data, size, reliable, channel);
}

char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize)
{
	return parsePacket(*event.packet, p, dataSize);
}

char *parsePacket(ENetPacket &packet, Packet &p, size_t &dataSize)
{
	size_t size = packet.dataLength;
	void *data = packet.data;
	dataSize = std::max(size_t(0), size - sizeof(Packet));

	memcpy(&p, data, sizeof(Packet));

	if (size <= sizeof(Packet))
	{
		return nullptr;
	}
	else
	{
		return (char *)data + sizeof(Packet);
	}
}
