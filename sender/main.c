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
	printf("Send compiled for %s\n", debrel);
	int client = RDT_socket(SINGLE_PACKET);

	RDT_bind(client, "localhost", 5791);
	RDT_connect(client, "localhost", 18752);

	RDT_close(client);
}