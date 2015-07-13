#ifndef _SOCKS5_H
#define _SOCKS5_H

#include <ev.h>



// public function
struct socks5_server* create_socks5_server(const char *addr, const char *port);
void clean_socks5_server(struct socks5_server* server);

// Structure

struct socks5_server {
    ev_io io;
    int fd;
    struct sockaddr *sockaddr;
    struct ev_loop *loop;
};

struct socks5_client {
	int fd;

};


#endif // _SOCKS5_H