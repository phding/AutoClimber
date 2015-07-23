#ifndef _SOCKS5_PROXY_H
#define _SOCKS5_PROXY_H

#include <stdbool.h>

#include "socks5.h"

void init_socks5_proxy();

struct proxy_node {
	ev_io io;
	bool connected;
	bool direct;
	void* proxy_client;

};

struct proxy_socks5_client{
	int yes;
};


struct proxy_direct_client{
    int fd;

};


#endif // _SOCKS5_PROXY_H
