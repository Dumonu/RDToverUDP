#ifndef SOCK_H_202004061734
#define SOCK_H_202004061734

enum RDT_Protocol {
	GOBACKN,
	SELECTIVE_REPEAT
};

int RDT_socket(enum RDT_Protocol protocol);
int RDT_bind(int pipe_idx, const char* addr, uint16_t port);
int RDT_listent(int pipe_idx, int backlog); // don't know if I need
int RDT_accept(int pipe_idx);
int RDT_connect(int pipe_idx, const char* addr, uint16_t port);
int RDT_send(int pipe_idx, const void* buf, size_t len);
int RDT_recv(int pipe_idx, void* buf, size_t len);
int RDT_close(int pipe_idx);
void RDT_info_addr_loc(int pipe_idx, char* buf, size_t len);
uint16_t RDT_info_port_loc(int pipe_idx);
void RDT_info_addr_rem(int pipe_idx, char* buf, size_t len);
uint16_t RDT_info_port_rem(int pipe_idx);
enum RDT_Protocol RDT_info_protocol(int pipe_idx);

#endif