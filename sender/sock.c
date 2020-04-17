#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <sys/type.h>
#include <sys/socket.h>

#include "global.h"
#include "sock.h"

// TODO: Think about whether you actually need more than one UDP socket for an
// arbitrary number of RDT connections

struct RDT_Pipe {
	int sock_fd;
	struct sockaddr_in local;
	struct sockaddr_in remote;

	enum RDT_Protocol protocol;
	/* TODO: add protocol implementation data here. */
};

// Internal Data Table
bool RDT_initialized = false;   // has the table been initialized?
size_t RDT_allocated = 0;		// Number of slots allocated in the table
struct RDT_Pipe* RDT_pipes = NULL; // The table itself

int RDT_socket(enum RDT_Protocol protocol)
{
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock_fd < 0){
		// socket creation failed
		DBG_FPRINTF(stderr, "createRDTPipe(): error creating socket: %s\n", strerr(errno));
		return -1;
	}

	if(!RDT_initialized){
		RDT_pipes = calloc(10, sizeof(*RDT_pipes));
		RDT_allocated = 10; // start by allocating 10 slots
		for(int i = 0; i < 10; ++i){
			RDT_pipes[i].sock_fd = -1; // invalid fd shows that slot empty
		}
		RDT_initialized = true;
	}

	// find earliest open slot
	int newIdx = -1;
	for(int i = 0; i < RDT_allocated; ++i){
		if(RDT_pipes[i].sock_fd == -1){
			newIdx = i;
			break;
		}
	}

	// if there aren't any, realloc, doubling size
	if(newIdx == -1){
		newIdx = RDT_allocated;
		RDT_allocated *= 2;
		RDT_pipes = realloc(RDT_pipes, RDT_allocated * sizeof(*RDT_pipes));
		for(int i = RDT_allocated / 2; i < RDT_allocated; ++i)
		{
			RDT_pipes[i].sock_fd = -1;
		}
	}

	RDT_pipes[newIdx].sock_fd = sock_fd;
	RDT_pipes[newIdx].protocol = protocol;
	/* TODO: Protocol data initialization here */

	return newIdx;
}

int RDT_bind(int pipe_idx, const char* addr, uint16_t port)
{

}

// This function may end up being removed - listen can be rolled into RDT_accept
int RDT_listen(int pipe_idx, int backlog)
{

}

int RDT_accept(int pipe_idx)
{

}

int RDT_connect(int pipe_idx, const char* addr, uint16_t port)
{
	
}

int RDT_send(int pipe_idx, const void *buf, size_t len)
{

}

int RDT_recv(int pipe_idx, void* buf, size_t len)
{

}

int RDT_close(int pipe_idx)
{

}

void RDT_info_addr_loc(int pipe_idx, char *buf, size_t len)
{

}

uint16_t RDT_info_port_loc(int pipe_idx)
{

}

void RDT_info_addr_rem(int pipe_idx, char *buf, size_t len)
{

}

uint16_t RDT_info_port_rem(int pipe_idx)
{

}

enum RDT_Protocol RDT_info_protocol(int pipe_idx)
{
	return RDT_pipes[pipe_idx].protocol;
}