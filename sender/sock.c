#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
//#include <sys/type.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "global.h"
#include "sock.h"

// TODO: Think about whether you actually need more than one UDP socket for an
// arbitrary number of RDT connections

struct RDT_Pipe
{
	int sock_fd;
	struct sockaddr_in local;
	struct sockaddr_in remote;

	uint8_t stateflags;

	enum RDT_Protocol protocol;
	/* TODO: add protocol implementation data here. */
};

// Internal Data Table
bool RDT_initialized = false;	   // has the table been initialized?
size_t RDT_allocated = 0;		   // Number of slots allocated in the table
struct RDT_Pipe *RDT_pipes = NULL; // The table itself

#define CREATED(i) ((RDT_pipes[i].stateflags & 0x01) > 0)
#define BOUND(i) ((RDT_pipes[i].stateflags & 0x02) > 0)
#define LISTENING(i) ((RDT_pipes[i].stateflags & 0x04) > 0)
#define CONNECTED(i) ((RDT_pipes[i].stateflags & 0x08) > 0)

#define CREATE(i) (RDT_pipes[i].stateflags |= 0x01)
#define BIND(i) (RDT_pipes[i].stateflags |= 0x01)
#define LISTEN(i) (RDT_pipes[i].stateflags |= 0x01)
#define CONNECT(i) (RDT_pipes[i].stateflags |= 0x01)

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
	return -1;
}

int RDT_accept(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	return -1;
}

int RDT_connect(int pipe_idx, const char *addr, uint16_t port)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	return -1;
}

int RDT_send(int pipe_idx, const void *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	return -1;
}

int RDT_recv(int pipe_idx, void *buf, size_t len)
{
	if (pipe_idx >= RDT_allocated)
		return -1;
	return -1;
}

void RDT_close(int pipe_idx)
{
	if (pipe_idx >= RDT_allocated)
		return;

	int ret = 0;
	if ((ret = close(RDT_pipes[pipe_idx].sock_fd)) != 0)
	{
		DBG_FPRINTF(stderr, "RDT_close(%d): %s\n", pipe_idx, strerror(errno));
	}
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