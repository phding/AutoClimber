#ifndef _SOCKS5_H
#define _SOCKS5_H

#include <ev.h>

// Structure

struct socks5_server {
    ev_io io;
    int fd;
    struct sockaddr *sockaddr;
    struct ev_loop *loop;
};

struct socket_io_handler {
	ev_io io;
	struct socks5_client* client;

};

struct socks5_client {
	int fd;
	struct socket_io_handler recv_handler;
	struct socket_io_handler send_handler;
};


// public function
struct socks5_server* create_socks5_server(const char *addr, const char *port);
void clean_socks5_server(struct socks5_server* server);

// external handler
void (*client_recv_handler)(struct socks5_client *client, int revents);
void (*client_send_handler)(struct socks5_client *client, int revents);


#endif // _SOCKS5_H