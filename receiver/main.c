#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "global.h"
#include "sock.h"

#ifdef DEBUG_
	char* debrel = "Debug";
#else
	char* debrel = "Release";
#endif

int server;
FILE* out;

void onsigint(int signum){
	if(RDT_info_created(server)){
		RDT_close(server);
	}

	if(out){
		fclose(out);
	}

	exit(1);
}

int main(int argc, char** argv)
{
	printf("Receive compiled for %s\n", debrel);

	if(argc != 4){
		fprintf(stderr, "Usage: %s <SP|SR> <listen_port> <save_file>\n", argv[0]);
		return 1;
	}

	enum RDT_Protocol protocol;
	if(strcmp(argv[1], "SP") == 0){
		protocol = SINGLE_PACKET;
	} else if(strcmp(argv[1], "SR") == 0){
		protocol = SELECTIVE_REPEAT;
	} else if(strcmp(argv[1], "GBN") == 0){
		fprintf(stderr, "Go Back N implemented in SeshensCode/send\n");
		return 1;
	} else {
		fprintf(stderr, "Protocol Unknown: %s\n", argv[1]);
		return 1;
	}

	signal(SIGINT, onsigint);

	server = RDT_socket(protocol);

	RDT_bind(server, "localhost", atoi(argv[2]));
	RDT_listen(server, 1);
	RDT_accept(server);

	out = fopen(argv[3], "wb");
	if(!out){
		fprintf(stderr, "Error opening %s: %s", argv[3], strerror(errno));
		RDT_close(server);
		return 1;
	}

	char msg[1000] = {0};
	int copied = -1;
	while(RDT_info_connected(server)){
		copied = RDT_recv(server, msg, 1000);
		DBG_PRINTF("Received %d bytes\n", copied);
		if(copied != 0){
#ifdef DEBUG_
			printf("{");
			for(int i = 0; i < copied; ++i){
				printf("%d, ", msg[i]);
			}
			printf("}\n");
#endif
			fwrite(msg, 1, copied, out);
		}
	}
	fclose(out);

	RDT_close(server);
}