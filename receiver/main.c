#include <stdio.h>
#include <stdint.h>

#include "global.h"
#include "sock.h"

#ifdef DEBUG_
	char* debrel = "Debug";
#else
	char* debrel = "Release";
#endif

int main(void)
{
	printf("Receive compiled for %s\n", debrel);
	int server = RDT_socket(SINGLE_PACKET);

	RDT_bind(server, "localhost", 18752);
	RDT_listen(server, 1);
	RDT_accept(server);

	char msg[100] = {0};
	RDT_recv(server, msg, 100);
	printf("%s\n", msg);

	RDT_close(server);
}