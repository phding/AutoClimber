
#include <fcntl.h>
#include <unistd.h> // close function
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <netdb.h>

#include "socks5.h"
#include "logging.h"

bool verbose = true;

int active_conn = 0;

// Function declaration
static struct socks5_client* create_socsk5_client(int fd);
static void accept_cb(EV_P_ ev_io *w, int revents);
static int create_and_bind(const char *addr, const char *port);
static void client_recv_cb(EV_P_ ev_io *w, int revents);
static void client_send_cb(EV_P_ ev_io *w, int revents);

// Socket Utilites
int setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void close_and_free_socks5_client(EV_P_ struct socks5_client* client)
{
    active_conn--;
    LOGI("Close socks5 client");
    if (client != NULL) {
        ev_io_stop(EV_A_ & client->recv_handler.io);
        ev_io_stop(EV_A_ & client->send_handler.io);
        close(client->fd);

        if (client->buf != NULL) {
            free(client->buf);
        }
        free(client);
    }
}

void close_and_free_socks5_server(struct socks5_server* server)
{
	// Clean up
    ev_io_stop(server->loop, &server->io);
    close(server->fd);

    //free(server->sockaddr);
    free(server);
}

// Socks5 handler
struct socks5_server* create_socks5_server(const char *addr, const char *port)
{

    signal(SIGPIPE, SIG_IGN);

	// Create socks5 server
	struct socks5_server *server;
	server = malloc(sizeof(struct socks5_server));
	memset(server, 0, sizeof(struct socks5_server));

	// Bind the specific port for TCP
	int listen_fd = create_and_bind(addr, port);
	if(listen_fd < 0){
		FATAL("create and bind error");
	}

    if (listen(listen_fd, SOMAXCONN) == -1) {
        FATAL("listen() error");
    }

	setnonblocking(listen_fd);
	server->fd = listen_fd;
    if(verbose){
        LOGI("TCP socsk5 enabled");
    }

	// Get libev to loop
	server->loop = EV_DEFAULT;
	ev_io_init(&server->io, accept_cb, server->fd, EV_READ);
    ev_io_start(server->loop, &server->io);

    // Setup UDP
    //LOGI("udprelay enabled");
    //init_udprelay(addr, port, listen_ctx.remote_addr[0], get_sockaddr_len(listen_ctx.remote_addr[0]), m, listen_ctx.timeout, iface);

	LOGI("Listening at %s:%s", addr, port);
	return server;
}

static int create_and_bind(const char *addr, const char *port)
{
	struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, listen_sock;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */

    s = getaddrinfo(addr, port, &hints, &result);
    if (s != 0) {
        LOGI("getaddrinfo: %s", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_sock == -1) {
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
        setsockopt(listen_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

        s = bind(listen_sock, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) {
            /* We managed to bind successfully! */
            break;
        } else {
            ERROR("bind");
        }

        close(listen_sock);
    }

    if (rp == NULL) {
        LOGE("Could not bind");
        return -1;
    }

    freeaddrinfo(result);

    return listen_sock;
}

static struct socks5_client* create_socsk5_client(int fd)
{
	struct socks5_client *client;
    client = malloc(sizeof(struct socks5_client));
    memset(client, 0, sizeof(struct socks5_client));


    client->buf = malloc(BUF_SIZE);
    client->recv_handler.client = client;
    client->send_handler.client = client;

    ev_io_init(&client->recv_handler.io, client_recv_cb, fd, EV_READ);
    ev_io_init(&client->send_handler.io, client_send_cb, fd, EV_WRITE);

    client->fd = fd;
    return client;
}

// Accept incoming connection
static void accept_cb(EV_P_ ev_io *w, int revents)
{
    LOGI("Accept, active conn = %d", active_conn);
    active_conn++;
    struct socks5_server *server = (struct socks5_server *)w;
    int client_fd = accept(server->fd, NULL, NULL);
    if (client_fd == -1) {
        ERROR("Unable to accept");
        return;
    }

    setnonblocking(client_fd);
    int opt = 1;
    setsockopt(client_fd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

    struct socks5_client *client = create_socsk5_client(client_fd);
    ev_io_start(EV_A_ & client->recv_handler.io);

}

// Handling client incoming message
static void client_recv_cb(EV_P_ ev_io *w, int revents)
{
    struct socket_io_handler* handler = (struct socket_io_handler*)w;
    struct socks5_client *client = (struct socks5_client *)handler->client;
    struct socks5_response response;

    client->buf_len = recv(client->fd, client->buf, BUF_SIZE, 0);

    if(client->buf_len == 0){
        // Connection is going to close
        return;
    }
    else if(client->buf_len < 0){
        // Error occur
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data
            LOGI("Wouldblock");
            
            // continue to wait for recv
            return;
        } else {
            LOGI("client_recv_io");
            ERROR("client_recv_io");
            close_and_free_socks5_client(EV_A_ client);
            // Close
            return;
        }
    }

    if(client->stage == 0){
        struct method_select_request *request = (struct method_select_request*)client->buf;
        // check version
        if(request->ver != SVERSION){
            // Unknown version
            LOGE("Unknown socks version: %x", request->ver);
            close_and_free_socks5_client(EV_A_ client);
            return;
        }

        // Send authentication mode
        struct method_select_response response;
        response.ver = SVERSION;
        response.method = AUTH_NO_REQUIRED;
        char *send_buf = (char *)&response;
        send(client->fd, send_buf, sizeof(response), 0);

        client->stage = 1;
        return;
    }
    else if(client->stage == 1){
        struct socks5_request *request = (struct socks5_request *)client->buf;
        client->stage = 2;
        if(client_recv_request_handler != NULL){
            (*client_recv_request_handler)(EV_A_ client, request);
        }else{
            // Send 
            response.ver = SVERSION;
            response.rep = RES_GENERAL_SOCKS_FAILURE;
            response.atyp = request->atyp;
            response.rsv = 0;
            char* send_buf = (char*)&response;
            send(client->fd, send_buf, sizeof(struct socks5_response), 0);
            close_and_free_socks5_client(EV_A_ client);
            return;
        }
    }else if(client->stage == 2){
        // Send and receive
        if(client_recv_data_handler != NULL){
            (*client_recv_data_handler)(EV_A_ client);
        }
    }else{
        LOGI("another stage");
        // Now runs 
    }
}

static void client_send_cb(EV_P_ ev_io *w, int revents)
{
    struct socket_io_handler* handler = (struct socket_io_handler*)w;
    struct socks5_client *client = (struct socks5_client *)handler->client;
    if(client_send_data_handler != NULL){
        (*client_send_data_handler)(EV_A_ client);
    }
}
