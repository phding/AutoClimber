#ifndef _SOCKS5_H
#define _SOCKS5_H

#include <ev.h>
#include <stdbool.h>

#define BUF_SIZE 2048
#define MAX_CONNECT_TIMEOUT 30 //30sec


#define SVERSION 0x05

// Command type
#define CMD_CONNECT 0x01
#define CMD_BIND 0x01
#define CMD_UDP_ASSOCIATE 0x01

// ATYP
#define IPV4 0x01
#define DOMAIN 0x03
#define IPV6 0x04

// Authentication
#define AUTH_NO_REQUIRED 0x00
#define AUTH_GSSAPI 0x01
#define AUTH_USERNAME_PASSWORD 0x02

// Response type
#define RES_SUCCEEDED 0x00
#define RES_GENERAL_SOCKS_FAILURE 0X01
#define RES_CONNECTION_NOT_ALLOWED_BY_RULESET 0X02
#define RES_NETWORK_UNREACHABLE 0X03
#define RES_HOST_UNREACHABLE 0X04
#define RES_CONNECTION_REFUSED 0X05
#define RES_TTL_EXPIRED 0X06
#define RES_CMD_NOT_SUPPORTED 0x07
#define RES_ADDRESS_TYPE_NOT_SUPPORT 0X08

#pragma pack(push)
#pragma pack(1)

extern bool verbose;
extern int active_conn;

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
    int recv_size;
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
void close_and_free_socks5_server(struct socks5_server* server);
void close_and_free_socks5_client(EV_P_ struct socks5_client* client);
int setnonblocking(int fd);

// external handler
void (*client_recv_request_handler)(EV_P_ struct socks5_client *client, struct socks5_request* request);
void (*client_recv_data_handler)(EV_P_ struct socks5_client *client);
void (*client_send_data_handler)(EV_P_ struct socks5_client *client);


#endif // _SOCKS5_H