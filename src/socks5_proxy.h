#ifndef _SOCKS5_PROXY_H
#define _SOCKS5_PROXY_H

#include <stdbool.h>
#include <sys/socket.h>

#include "socks5.h"

void init_socks5_proxy();

struct proxy_node {
	ev_io io;
	bool connected;
	bool direct;
	struct proxy_remote_client* remote_client;
	struct socks5_client* socks5_client;

};


struct proxy_io_handler{
	ev_io io;
	ev_timer watcher;
	struct proxy_remote_client* remote;
};


struct proxy_remote_client{
	int fd;
	char* buf;
    int recv_size;
	struct sockaddr addr;
	struct proxy_node* node;
	int addr_len;
	struct proxy_io_handler recv_handler;
	struct proxy_io_handler send_handler;
};


#endif // _SOCKS5_PROXY_H
