#ifndef SOCK_H_202004061734
#define SOCK_H_202004061734

enum RDT_Protocol {
	SINGLE_PACKET,
	GOBACKN,
	SELECTIVE_REPEAT
};

// ACTIONS
int RDT_socket(enum RDT_Protocol protocol);
int RDT_bind(int pipe_idx, const char* addr, uint16_t port);
int RDT_listen(int pipe_idx, int backlog); // don't know if I need
int RDT_accept(int pipe_idx);
int RDT_connect(int pipe_idx, const char* addr, uint16_t port);
int RDT_send(int pipe_idx, const void* buf, size_t len);
int RDT_recv(int pipe_idx, void* buf, size_t len);
void RDT_close(int pipe_idx);

// INFO
int RDT_info_addr_loc(int pipe_idx, char* buf, size_t len);
uint16_t RDT_info_port_loc(int pipe_idx);
int RDT_info_addr_rem(int pipe_idx, char* buf, size_t len);
uint16_t RDT_info_port_rem(int pipe_idx);
enum RDT_Protocol RDT_info_protocol(int pipe_idx);

// STATE FLAGS
bool RDT_info_created(int pipe_idx);
bool RDT_info_bound(int pipe_idx);
bool RDT_info_listening(int pipe_idx);
bool RDT_info_connected(int pipe_idx);

#endif