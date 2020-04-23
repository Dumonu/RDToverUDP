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

	char msg[100] = {0};
	scanf("%99s ", msg);
	RDT_send(client, msg, 100);

	RDT_close(client);
}