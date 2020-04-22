#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include <unistd.h>
//#include <sys/type.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "global.h"
#include "sock.h"

struct RDT_Pipe
{
	int sock_fd;
	struct sockaddr_in local;
	struct sockaddr_in remote;

	// Bit 0: Socket Created (S)
	// Bit 1: Socket Bound (B)
	// Bit 2: Socket Listening (L)
	// Bit 3: Socket Connected (C) (either connect or accept)
	// Bit 4: Local Side FIN (F)
	// Bit 5: Remote Side FIN (A) - for ACK
	// 00AFCLBS
	uint8_t stateflags;

	uint32_t msec_timeout;

	/* TODO: add backlog things here */

	enum RDT_Protocol protocol;
	/* TODO: add protocol implementation data here. */
	uint8_t loc_seq;
	uint8_t rem_seq;
};

struct RDT_Header
{
	uint8_t seqnum;	// sequence number of this packet
	uint8_t acknum; // sequence number this packet is ACKing

	// Bit 0: FIN
	// Bit 1: SYN
	// Bit 2: RST
	// Bit 3: PSH (Not Used)
	// Bit 4: ACK
	// Bit 5: URG (Not Used)
	// 000A0RSF
	uint8_t flags; 
	uint8_t rwnd; // receiver window - number of 100-byte packets receiver can accept
	uint16_t checksum; // computed checksum - in network order
};

struct RDT_Packet
{
	struct RDT_Header header;
	char payload[100]; // should be zero-padded if not full
};

struct RDT_PacketListEntry
{
	uint8_t seqnum;
	bool acked;
	struct RDT_Packet packet;
};

// Internal Data Table
bool RDT_initialized = false;	   // has the table been initialized?
size_t RDT_allocated = 0;		   // Number of slots allocated in the table
struct RDT_Pipe *RDT_pipes = NULL; // The table itself

#define CREATED(i) ((RDT_pipes[i].stateflags & 0x01) > 0)
#define BOUND(i) ((RDT_pipes[i].stateflags & 0x02) > 0)
#define LISTENING(i) ((RDT_pipes[i].stateflags & 0x04) > 0)
#define CONNECTED(i) ((RDT_pipes[i].stateflags & 0x08) > 0)
#define LOCALCLOSED(i) ((RDT_pipes[i].stateflags & 0x10) > 0)
#define REMOTECLOSED(i) ((RDT_pipes[i].stateflags & 0x20) > 0)

#define CREATE(i) (RDT_pipes[i].stateflags |= 0x01)
#define BIND(i) (RDT_pipes[i].stateflags |= 0x02)
#define LISTEN(i) (RDT_pipes[i].stateflags |= 0x04)
#define CONNECT(i) (RDT_pipes[i].stateflags |= 0x08)
#define LOCALCLOSE(i) (RDT_pipes[i].stateflags |= 0x10)
#define REMOTECLOSE(i) (RDT_pipes[i].stateflags |= 0x20)

/**
 * Expects a buffer in network byte order.
 * If the buffer has a zero-filled checksum field, this will compute the checksum.
 * If the buffer does not have a zero-filled checksum field, this will check the
 * checksum. If the result is 0, the check passes.
 **/
uint16_t RDT_inet_chksum(void* buf, size_t len)
{
	// convert to 16-bit words
	uint16_t* words = (uint16_t*)buf;
	size_t wlen = len / sizeof(*words);
	// sum all words
	uint32_t sum = 0;
	for(int i = 0; i < wlen; ++i){
		sum += ntohs(words[i]);
	}
	sum = (sum & 0x0000FFFF) + (sum & 0xFFFF0000); // add carry over
	sum = (sum & 0x0000FFFF) + (sum & 0xFFFF0000); // add carry over of previous step
	uint16_t chksum = (uint16_t)sum ^ 0xFFFF; // Truncate and flip all bits
	return chksum;
}

int RDT_waitForData(int pipe_idx)
{
	fd_set socks;
	FD_ZERO(&socks);
	FD_SET(RDT_pipes[pipe_idx].sock_fd, &socks);
	struct timeval timeout = {0};
	timeout.tv_sec = RDT_pipes[pipe_idx].msec_timeout / 1000;
	timeout.tv_usec = (RDT_pipes[pipe_idx].msec_timeout % 1000) * 1000;

	return select(
		RDT_pipes[pipe_idx].sock_fd + 1,
		&socks,
		NULL,
		NULL,
		&timeout
	);
}

int RDT_socket(enum RDT_Protocol protocol)
{
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0)
	{
		// socket creation failed
		DBG_FPRINTF(stderr, "createRDTPipe(): error creating socket: %s\n",
			strerror(errno));
		return -1;
	}

	if (!RDT_initialized)
	{
		srand(time(NULL));
		RDT_pipes = calloc(10, sizeof(*RDT_pipes));
		RDT_allocated = 10; // start by allocating 10 slots
		RDT_initialized = true;
	}

	// find earliest open slot
	int newIdx = -1;
	for (int i = 0; i < RDT_allocated; ++i)
	{
		if (!CREATED(i))
		{
			newIdx = i;
			break;
		}
	}

	// if there aren't any, realloc, doubling size
	if (newIdx == -1)
	{
		newIdx = RDT_allocated;
		RDT_allocated *= 2;
		RDT_pipes = realloc(RDT_pipes, RDT_allocated * sizeof(*RDT_pipes));
		// This ensures flags are fully cleared - no zombie slots
		memset(
			RDT_pipes + (RDT_allocated / 2),
			0,
			(RDT_allocated / 2) * sizeof(*RDT_pipes)
		);
	}

	RDT_pipes[newIdx].sock_fd = sock_fd;
	RDT_pipes[newIdx].protocol = protocol;

	// TODO: change this for real things
	RDT_pipes[newIdx].msec_timeout = 1000;
	/* TODO: Protocol data initialization here */
	CREATE(newIdx);

	return newIdx;
}

int RDT_bind(int pipe_idx, const char *addr, uint16_t port)
{
	if (pipe_idx >= RDT_allocated)
		return -1;

	// socket either doesn't exist or is already bound
	if (!CREATED(pipe_idx) || BOUND(pipe_idx))
		return -1;

	// populate the local address to which to bind
	RDT_pipes[pipe_idx].local.sin_family = AF_INET;
	RDT_pipes[pipe_idx].local.sin_port = port;
	inet_aton(addr, &RDT_pipes[pipe_idx].local.sin_addr);
	memset(RDT_pipes[pipe_idx].local.sin_zero, 0, 8);

	// bind the socket to the address
	int ret = bind(
		RDT_pipes[pipe_idx].sock_fd,
		(struct sockaddr *)&RDT_pipes[pipe_idx].local,
		sizeof(struct sockaddr_in)
	);
	if (ret != 0)
	{
		DBG_FPRINTF(stderr, "RDT_bind(%d, \"%s\", %d):\n\t%s", pipe_idx, addr, port,
			strerror(errno));
		return errno;
	}
	BIND(pipe_idx);
	return 0;
}

// This function may end up being removed - listen can be rolled into RDT_accept
int RDT_listen(int pipe_idx, int backlog)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	if (!CREATED(pipe_idx) || !BOUND(pipe_idx))
		return -1;

	/* listen() normally only works for SOCK_STREAM or SOCK_SEQPACKET,
	   so this will do the same thing for our RDT sockets. */
	// TODO: Set up backlog things
	LISTEN(pipe_idx);
	return 0;
}

int RDT_accept(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return -1;

	if(!CREATED(pipe_idx) || !BOUND(pipe_idx) || !LISTENING(pipe_idx) ||
			CONNECTED(pipe_idx))
		return -1;

	struct RDT_Packet syn = {0};
	struct sockaddr_in cli_addr = {0};
	socklen_t cli_addr_len = sizeof(cli_addr);
	bool wait = true;
	do{
		int ret = recvfrom(
			RDT_pipes[pipe_idx].sock_fd,
			&syn,
			sizeof(syn),
			0,
			(struct sockaddr *)&cli_addr,
			&cli_addr_len
		);
		if(ret != sizeof(syn)){
			DBG_FPRINTF(stderr, "RDT_accept: Incoming connection not valid\n");
			continue;
		}
		if(RDT_inet_chksum(&syn, sizeof(syn)) != 0){
			DBG_FPRINTF(stderr, "RDT_accept: Incoming packet failed checksum\n");
			continue;
		}
		if(syn.header.flags & 2 != 2){
			DBG_FPRINTF(stderr, "RDT_accept: Incoming packet is not a SYN\n");
			continue;
		}

		DBG_PRINTF("RDT_accept: Received SYN from %s:%d\n", inet_ntoa(cli_addr.sin_addr), 
			cli_addr.sin_port);

		wait = false;
	} while(wait);

	// See comment below
	connect(RDT_pipes[pipe_idx].sock_fd, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
	RDT_pipes[pipe_idx].remote = cli_addr; // should be trivially copyable
	RDT_pipes[pipe_idx].loc_seq = rand() % 256;
	RDT_pipes[pipe_idx].rem_seq = syn.header.seqnum;

	struct RDT_Packet synack = {0};
	synack.header.seqnum = RDT_pipes[pipe_idx].loc_seq;
	synack.header.acknum = RDT_pipes[pipe_idx].rem_seq;
	synack.header.flags = 0x12; // 00010010
	synack.header.rwnd = 1; // TODO: protocol defined; maybe leave as is for 3wh
	int chksum = RDT_inet_chksum(&synack, sizeof(synack));
	synack.header.checksum = htons(chksum);
#ifdef DEBUG_
	assert(RDT_inet_chksum(&synack, sizeof(synack)) == 0);
#endif

	// Transmit SYNACK message and wait for ACK
	bool retransmit = true;
	while(retransmit){
		DBG_PRINTF("RDT_accept: Sending SYNACK response to %s:%d\n",
			inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
		int ret = send(RDT_pipes[pipe_idx].sock_fd, &synack, sizeof(synack), 0);
		if(ret != sizeof(synack)){
			DBG_FPRINTF(stderr, "RDT_accept: SYNACK message did not send correct "
				"number of bytes.\n");
			return -1;
		}
		struct RDT_Packet ack = {0};

		ret = RDT_waitForData(pipe_idx);
		if(ret == 0){
			DBG_PRINTF("RDT_accept: Timeout waiting for ACK\n");
			continue;
		} else if(ret < 0){
			DBG_FPRINTF(stderr, "RDT_accept: Error waiting for ACK: %s\n",
				strerror(errno));
				return -1;
		}

		ret = recv(RDT_pipes[pipe_idx].sock_fd, &ack, sizeof(ack), 0);
		if(ret == -1){
			DBG_FPRINTF(stderr, "RDT_accept: Error reading ACK: %s", strerror(errno));
			return -1;
		}

		if(RDT_inet_chksum(&ack, sizeof(ack)) != 0){
			DBG_PRINTF("RDT_accept: Message received corrupt\n");
			continue;
		}
		
		if((ack.header.flags & 0x10) != 0x10){
			DBG_PRINTF("RDT_accept: Message received not an ACK\n");
			continue;
		}

		if(ack.header.acknum != RDT_pipes[pipe_idx].loc_seq){
			DBG_PRINTF("RDT_accept: Message received ACKing incorrect seqnum\n");
			continue;
		}

		DBG_PRINTF("RDT_accept: Received ACK from %s:%d\n", inet_ntoa(cli_addr.sin_addr),
			cli_addr.sin_port);
		retransmit = false;
	}
	RDT_pipes[pipe_idx].loc_seq++;
	CONNECT(pipe_idx);
	return pipe_idx;
}

// FIXME: Update connect to allow a port change for accept()'s new socket
int RDT_connect(int pipe_idx, const char *addr, uint16_t port)
{
	if (pipe_idx >= RDT_allocated){
		DBG_FPRINTF(stderr, "RDT_connect: %d >= RDT_allocated\n", pipe_idx);
		return -1;
	}
	if(!CREATED(pipe_idx) || !BOUND(pipe_idx) || CONNECTED(pipe_idx))
	{
		DBG_FPRINTF(stderr, "0x%x\n", RDT_pipes[pipe_idx].stateflags);
		DBG_FPRINTF(stderr, "RDT_connect: %d not created or not bound or already"
			" connected\n", pipe_idx);
		return -1;
	}

	// populate the remote address to which to connect
	RDT_pipes[pipe_idx].remote.sin_family = AF_INET;
	RDT_pipes[pipe_idx].remote.sin_port = port;
	inet_aton(addr, &RDT_pipes[pipe_idx].remote.sin_addr);
	memset(RDT_pipes[pipe_idx].remote.sin_zero, 0, 8);

	// From man 2 connect:
	// "If the socket sockfd is of type SOCK_DGRAM, then addr is the address to which
	// datagrams are sent by default, and the only address from which datagrams are
	// received"
	// This is wonderful. It will allow automatic source verification.
	connect(
		RDT_pipes[pipe_idx].sock_fd,
		(struct sockaddr*)&RDT_pipes[pipe_idx].remote,
		sizeof(struct sockaddr_in)
	);
	RDT_pipes[pipe_idx].loc_seq = rand() % 256; // choose initial sequence number

	struct RDT_Packet syn = {0};
	syn.header.seqnum = RDT_pipes[pipe_idx].loc_seq;
	syn.header.flags |= 2; // SYN bit
	syn.header.rwnd = 1; // TODO: protocol defined; maybe leave as is for 3wh
	uint16_t ckh = RDT_inet_chksum(&syn, sizeof(syn));
	syn.header.checksum = htons(ckh);

#ifdef DEBUG_
	// Verify checksum computed properly
	assert(RDT_inet_chksum(&syn, sizeof(syn)) == 0);
#endif

	// Transmit SYN message and wait for SYNACK
	bool retransmit = true;
	while(retransmit){
		DBG_PRINTF("RDT_Connect: Sending SYN request to %s:%d\n", addr, port);
		int ret = send(RDT_pipes[pipe_idx].sock_fd, &syn, sizeof(syn), 0);
		if(ret != sizeof(syn)){
			DBG_FPRINTF(stderr, "RDT_Connect: SYN message did not send correct "
				"number of bytes.\n");
			return -1;
		}
		struct RDT_Packet synack = {0};

		ret = RDT_waitForData(pipe_idx);
		if(ret == 0){
			DBG_PRINTF("RDT_Connect: Timeout waiting for SYNACK\n");
			continue;
		} else if(ret < 0){
			DBG_FPRINTF(stderr, "RDT_Connect: Error waiting for SYNACK: %s\n",
				strerror(errno));
			return -1;
		}

		ret = recv(RDT_pipes[pipe_idx].sock_fd, &synack, sizeof(synack), 0);
		if(ret == -1){
			DBG_FPRINTF(stderr, "RDT_Connect: Error reading SYNACK: %s", strerror(errno));
			return -1;
		}

		if(RDT_inet_chksum(&synack, sizeof(synack)) != 0){
			DBG_PRINTF("RDT_Connect: Message received corrupt\n");
			continue;
		}
		
		if((synack.header.flags & 0x12) != 0x12){
			DBG_PRINTF("RDT_Connect: Message received not a SYNACK\n");
			continue;
		}

		if(synack.header.acknum != RDT_pipes[pipe_idx].loc_seq){
			DBG_PRINTF("RDT_Connect: Message received ACKing incorrect seqnum\n");
			continue;
		}

		DBG_PRINTF("RDT_Connect: Received SYNACK from %s:%d\n", addr, port);
		RDT_pipes[pipe_idx].rem_seq = synack.header.seqnum;
		retransmit = false;
	}

	// SYN sent, SYNACK received
	struct RDT_Packet ack = {0};
	ack.header.acknum = RDT_pipes[pipe_idx].rem_seq;
	ack.header.flags = 0x10;
	ack.header.rwnd = 1; //TODO: same as above
	ckh = RDT_inet_chksum(&ack, sizeof(ack));
	ack.header.checksum = htons(ckh);
#ifdef DEBUG_
	// Verify checksum computed properly
	assert(RDT_inet_chksum(&ack, sizeof(ack)) == 0);
#endif

	DBG_PRINTF("RDT_Connect: Sending ACK to %s:%d\n", addr, port);
	int ret = send(RDT_pipes[pipe_idx].sock_fd, &ack, sizeof(ack), 0);
	if(ret != sizeof(ack)){
		DBG_FPRINTF(stderr, "RDT_Connect: Error sending ACK: %s\n", strerror(errno));
		return -1;
	}
	CONNECT(pipe_idx);
	return 0;
}

// Sending algorithm for Single Packet RDT Protocol
int RDT_send_SP(int pipe_idx, struct RDT_PacketListEntry* packlist, int list_len)
{
	for(int i = 0; i < list_len; ++i){
		bool resend = true;
		while(resend){
			DBG_PRINTF("Sending packet %d to %s:%d\n", packlist[i].seqnum,
				inet_ntoa(RDT_pipes[pipe_idx].remote.sin_addr),
				RDT_pipes[pipe_idx].remote.sin_port);

			int ret = send(RDT_pipes[pipe_idx].sock_fd, &packlist[i].packet,
				sizeof(packlist[i].packet), 0);
			if(ret != sizeof(packlist[i].packet)){
				DBG_FPRINTF(stderr, "RDT_send_SP: Error sending packet: %s\n",
					strerror(errno));
				continue;
			}

			ret = RDT_waitForData(pipe_idx);
			if(ret == 0){
				DBG_PRINTF("RDT_send_SP: Timeout waiting for ACK\n");
				continue;
			} else if(ret < 0){
				DBG_FPRINTF(stderr, "RDT_send_SP: Error waiting for ACK: %s\n",
					strerror(errno));
				continue; // Should this ever happen?
			}

			struct RDT_Packet ack = {0};
			ret = recv(RDT_pipes[pipe_idx].sock_fd, &ack, sizeof(ack), 0);
			if(ret == -1){
				DBG_FPRINTF(stderr, "RDT_send_SP: Error reading ACK: %s\n",
					strerror(errno));
				continue; // As before, if we have stuff to read, should this ever happen?
			}

			if(RDT_inet_chksum(&ack, sizeof(ack)) != 0){
				DBG_PRINTF("RDT_send_SP: ACK failed checksum\n");
				continue;
			}

			if((ack.header.flags & 0x10) != 0x10){
				DBG_PRINTF("RDT_send_SP: Message received not an ACK\n");
				continue;
			}

			if(ack.header.acknum != packlist[i].seqnum){
				DBG_PRINTF("RDT_send_SP: Message received ACKing incorrect seqnum\n");
				continue;
			}

			DBG_PRINTF("RDT_send_SP: Received ACK for %d from %s:%d\n", packlist[i].seqnum,
				inet_ntoa(RDT_pipes[pipe_idx].remote.sin_addr),
				RDT_pipes[pipe_idx].remote.sin_port);
			RDT_pipes[pipe_idx].loc_seq++;
			resend = false;
		}
	}
	return 0;
}

int RDT_send_gbN(int pipe_idx, struct RDT_PacketListEntry* packlist, int list_len)
{
	return -1;
}

int RDT_send_SR(int pipe_idx, struct RDT_PacketListEntry* packlist, int list_len)
{
	return -1;
}

// These next two are highly dependent on the protocol
int RDT_send(int pipe_idx, const void *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	if(!CREATED(pipe_idx) || !BOUND(pipe_idx) || !CONNECTED(pipe_idx))
		return -1;

	int list_len = len / 100 + ((len % 100) > 0 ? 1 : 0);
	struct RDT_PacketListEntry *packlist = calloc(list_len, sizeof(*packlist));
	// populate packet list
	for(size_t i = 0, p = 0; i < list_len && p < len; ++i, p += 100){
		DBG_PRINTF("RDT_send: Creating packet %d\n",
			(int)(RDT_pipes[pipe_idx].loc_seq + i));
		packlist[i].seqnum = RDT_pipes[pipe_idx].loc_seq + i;
		packlist[i].acked = 0;

		packlist[i].packet.header.seqnum = packlist[i].seqnum;
		packlist[i].packet.header.acknum = 0;
		packlist[i].packet.header.rwnd = 1; // TODO: Update for various protocols
		packlist[i].packet.header.flags = 0;
		// copy up to 100 bytes from buf to the payload
		memcpy(&packlist[i].packet.payload, buf + p, min(100, len - p));

		uint16_t chksum = RDT_inet_chksum(&packlist[i].packet, sizeof(packlist[i].packet));
		packlist[i].packet.header.checksum = htons(chksum);
#ifdef DEBUG_
		assert(RDT_inet_chksum(&packlist[i].packet, sizeof(packlist[i].packet)) == 0);
#endif
	}

	DBG_PRINTF("RDT_send: Packet List created\n");
	switch(RDT_pipes[pipe_idx].protocol)
	{
		case SINGLE_PACKET:
			RDT_send_SP(pipe_idx, packlist, list_len);
			break;
		case GOBACKN:
			RDT_send_gbN(pipe_idx, packlist, list_len);
			break;
		case SELECTIVE_REPEAT:
			RDT_send_SR(pipe_idx, packlist, list_len);
			break;
		default:
			DBG_FPRINTF(stderr, "RDT_send: Invalid protocol: %d\n",
				RDT_pipes[pipe_idx].protocol);
			break;
	}

	free(packlist);
}

int RDT_recv(int pipe_idx, void *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	return -1;
}

// TODO: Handle ACKing other side until it finishes
void RDT_close(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return;

	if(CONNECTED(pipe_idx)){
		// Implement finishing handshakes

		struct RDT_Packet fin = {0};
		fin.header.seqnum = RDT_pipes[pipe_idx].loc_seq;
		fin.header.flags = 0x01;
		fin.header.rwnd = 1;
		uint16_t chksum = RDT_inet_chksum(&fin, sizeof(fin));
		fin.header.checksum = htons(chksum);
#ifdef DEBUG_
		assert(RDT_inet_chksum(&fin, sizeof(fin)) == 0);
#endif

		struct RDT_Packet remfin = {0};
		// Transmit SYN message and wait for SYNACK
		bool retransmit = true;
		while (retransmit)
		{
			DBG_PRINTF("RDT_close: Sending FIN request to %s:%d\n",
				inet_ntoa(RDT_pipes[pipe_idx].remote.sin_addr),
				RDT_pipes[pipe_idx].remote.sin_port
			);

			int ret = send(RDT_pipes[pipe_idx].sock_fd, &fin, sizeof(fin), 0);
			if (ret != sizeof(fin))
			{
				DBG_FPRINTF(stderr, "RDT_close: SYN message did not send correct "
					"number of bytes.\n");
				continue;
			}
			struct RDT_Packet ack = {0};

			ret = RDT_waitForData(pipe_idx);
			if (ret == 0)
			{
				DBG_PRINTF("RDT_close: Timeout waiting for ACK\n");
				continue;
			}
			else if (ret < 0)
			{
				DBG_FPRINTF(stderr, "RDT_close: Error waiting for ACK: %s\n",
					strerror(errno));
				continue;
			}

			ret = recv(RDT_pipes[pipe_idx].sock_fd, &ack, sizeof(ack), 0);
			if (ret == -1)
			{
				DBG_FPRINTF(stderr, "RDT_close: Error reading ACK: %s", strerror(errno));
				continue;
			}

			if (RDT_inet_chksum(&ack, sizeof(ack)) != 0)
			{
				DBG_PRINTF("RDT_close: Message received corrupt\n");
				continue;
			}

			if ((ack.header.flags & 0x10) != 0x10)
			{
				if((ack.header.flags & 0x01) == 0x01){
					DBG_PRINTF("RDT_close: Message received is a FIN\n");
					remfin = ack;
					struct RDT_Packet locack = {0};
					locack.header.acknum = remfin.header.seqnum;
					locack.header.rwnd = 0;
					locack.header.flags = 0x10;
					uint16_t chksum = RDT_inet_chksum(&locack, sizeof(locack));
					locack.header.checksum = htons(chksum);
#ifdef DEBUG_
					assert(RDT_inet_chksum(&locack, sizeof(locack)) == 0);
#endif
					send(RDT_pipes[pipe_idx].sock_fd, &locack, sizeof(locack), 0);
					REMOTECLOSE(pipe_idx);
				}
				DBG_PRINTF("RDT_close: Message received not an ACK\n");
				continue;
			}

			if (ack.header.acknum != RDT_pipes[pipe_idx].loc_seq)
			{
				DBG_PRINTF("RDT_close: Message received ACKing incorrect seqnum\n");
				continue;
			}

			DBG_PRINTF("RDT_close: Received ACK from %s:%d\n", 
				inet_ntoa(RDT_pipes[pipe_idx].remote.sin_addr),
				RDT_pipes[pipe_idx].remote.sin_port
			);
			LOCALCLOSE(pipe_idx);
			retransmit = false;
		}
		if(!REMOTECLOSED(pipe_idx) && (remfin.header.flags & 0x01) == 0){
			int ret = recv(RDT_pipes[pipe_idx].sock_fd, &remfin, sizeof(remfin), 0);
			if(ret == -1){
				DBG_PRINTF("RDT_close: Error receiving FIN message:%s\n", strerror(errno));
				REMOTECLOSE(pipe_idx);
			}
		}
		if(!REMOTECLOSED(pipe_idx)){
			// FIXME: I REALLY JUST WANT TO ACK WHATEVER AND PRETEND IT'S A FIN
			struct RDT_Packet ack = {0};
			ack.header.acknum = remfin.header.seqnum;
			ack.header.rwnd = 0;
			ack.header.flags = 0x10;
			uint16_t chksum = RDT_inet_chksum(&ack, sizeof(ack));
			ack.header.checksum = htons(chksum);
#ifdef DEBUG_
			assert(RDT_inet_chksum(&ack, sizeof(ack)) == 0);
#endif
			send(RDT_pipes[pipe_idx].sock_fd, &ack, sizeof(ack), 0);
			REMOTECLOSE(pipe_idx);
		}
	}

	int ret = 0;
	if ((ret = close(RDT_pipes[pipe_idx].sock_fd)) != 0)
	{
		DBG_FPRINTF(stderr, "RDT_close(%d): %s\n", pipe_idx, strerror(errno));
	}
	memset(RDT_pipes + pipe_idx, 0, sizeof(*RDT_pipes));
}

int RDT_info_addr_loc(int pipe_idx, char *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;

	if (!BOUND(pipe_idx))
		return -1;

	char *addr = inet_ntoa(RDT_pipes[pipe_idx].local.sin_addr);
	strncpy(buf, addr, len);
	buf[len - 1] = 0; // ensure zero-terminated
	return strlen(buf) + 1;
}

uint16_t RDT_info_port_loc(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return 0;

	if (!BOUND(pipe_idx))
		return 0;

	return ntohs(RDT_pipes[pipe_idx].local.sin_port);
}

int RDT_info_addr_rem(int pipe_idx, char *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;

	if (!CONNECTED(pipe_idx))
		return -1;

	char *addr = inet_ntoa(RDT_pipes[pipe_idx].local.sin_addr);
	strncpy(buf, addr, len);
	buf[len - 1] = 0; // ensure zero-terminated
	return strlen(buf) + 1;
}

uint16_t RDT_info_port_rem(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return 0;

	if (!CONNECTED(pipe_idx))
		return 0;

	return ntohs(RDT_pipes[pipe_idx].remote.sin_port);
}

enum RDT_Protocol RDT_info_protocol(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return -1;

	return RDT_pipes[pipe_idx].protocol;
}

bool RDT_info_created(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return false;

	return CREATED(pipe_idx);
}

bool RDT_info_bound(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return false;

	return BOUND(pipe_idx);
}

bool RDT_info_listening(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return false;

	return LISTENING(pipe_idx);
}

bool RDT_info_connected(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return false;

	return CONNECTED(pipe_idx);
}