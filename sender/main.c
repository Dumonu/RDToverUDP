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

int client;
FILE* in;

void onsigint(int signum){
	if(RDT_info_created(client)){
		RDT_close(client);
	}

	if(in){
		fclose(in);
	}

	exit(1);
}

int main(int argc, char** argv)
{
	printf("Send compiled for %s\n", debrel);

	if(argc != 5){
		fprintf(stderr, "Usage: %s <SP|SR> <recv_host> <recv_port> <send_file>\n", argv[0]);
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

	int client = RDT_socket(protocol);

	RDT_bind(client, "localhost", 5791);
	RDT_connect(client, argv[2], atoi(argv[3]));

	FILE* in = fopen(argv[4], "rb");
	if(!in){
		fprintf(stderr, "Error opening %s: %s\n", argv[4], strerror(errno));
		RDT_close(client);
		return 1;
	}

	int ret = fseek(in, 0, SEEK_END);
	if(ret != 0){
		fprintf(stderr, "Error finding end of file: %s\n", strerror(errno));
		RDT_close(client);
		return 1;
	}
	int len = ftell(in);
	if(len < 0){
		fprintf(stderr, "Error finding end of file: %s\n", strerror(errno));
		RDT_close(client);
		return 1;
	}
	fseek(in, 0, SEEK_SET);

	char* msg = malloc(len);
	ret = fread(msg, 1, len, in);
	if(ret != len){
		fprintf(stderr, "Error reading file\n");
		free(msg);
		fclose(in);
		RDT_close(client);
		return 1;
	}

	RDT_send(client, msg, len);

	free(msg);
	fclose(in);

	RDT_close(client);
}