#include "packet.h"
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>

struct packet createPacket(int flags, int id, int seq, int windowsize, int crc, char *data) {
	struct packet packet;
	packet.flags = flags;
	packet.id = id;
	packet.seq = seq;
	packet.windowsize = windowsize;
	packet.crc = crc;
	packet.data = data;
	return packet;
}

int receivePacketWithTimeout(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout) {
	struct timeval timevalue;
	int readyFDs;
	timevalue.tv_sec = 0;
	timevalue.tv_usec = timeout * 1000;
	fd_set fdSet;
	
	FD_ZERO(&fdSet);
	FD_SET(mySocket, &fdSet);
	
	readyFDs = select(mySocket + 1, &fdSet, NULL, NULL, &timevalue);
	if (readyFDs == -1) {
		perror("Failed to receive packet");
		exit(EXIT_FAILURE);
	}
	else if (readyFDs == 0)
		return 1;
	receivePacket(mySocket, packet, source);
	return 0;
}

void receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source) {
	socklen_t addressSize = 0;
	if (packet != NULL)
		addressSize = sizeof(*source);
	if (recvfrom(mySocket, packet, sizeof(struct packet), 0, (struct sockaddr*)source, &addressSize) == -1) {
		perror("Failed to receive packet");
		exit(EXIT_FAILURE);
	}
}

<<<<<<< HEAD
int sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination) {
	//int chance = rand()%2;
	//if (chance == 0)
	//	return 0;
	return sendto(mySocket, packet, sizeof(*packet), 0, (struct sockaddr*)destination, sizeof(*destination));
=======
void sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination) {
	/*int chance = rand()%2;
	if (chance == 0)
		return 0;*/
	if (sendto(mySocket, packet, sizeof(*packet), 0, (struct sockaddr*)destination, sizeof(*destination)) == -1) {
		perror("Failed to send packet");
		exit(EXIT_FAILURE);
	}
>>>>>>> 848fd7a6bd8791fbef7e7b502e5097f622aa55d5
}
