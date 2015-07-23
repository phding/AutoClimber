#ifndef _SOCKS5_H
#define _SOCKS5_H

#include <ev.h>
#include <stdbool.h>

#define BUF_SIZE 2048


#define SVERSION 0x05
#define CONNECT 0x01
#define IPV4 0x01
#define DOMAIN 0x03
#define IPV6 0x04
#define CMD_NOT_SUPPORTED 0x07

#pragma pack(push)
#pragma pack(1)

extern bool verbose;

// Structure
struct socks5_server {
    ev_io io;
    int fd;
    //struct sockaddr *sockaddr;
    struct ev_loop *loop;
};

struct socket_io_handler {
	ev_io io;
	struct socks5_client* client;

};

struct socks5_client {
	int fd;
    int stage;
	struct socket_io_handler recv_handler;
	struct socket_io_handler send_handler;
	char* buf;
	void* ptr;
};

struct method_select_request {
    char ver;
    char nmethods;
    char methods[255];
};

struct method_select_response {
    char ver;
    char method;
};

struct socks5_request {
    char ver;
    char cmd;
    char rsv;
    char atyp;
};

struct socks5_response {
    char ver;
    char rep;
    char rsv;
    char atyp;
};



// public function
struct socks5_server* create_socks5_server(const char *addr, const char *port);
void clean_socks5_server(struct socks5_server* server);

// external handler
void (*client_recv_handler)(struct socks5_client *client, int revents);
void (*client_send_handler)(struct socks5_client *client, int revents);


#endif // _SOCKS5_H