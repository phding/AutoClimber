
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


// Function declaration
static struct socks5_client* create_socsk5_client(int fd);
static void accept_cb(EV_P_ ev_io *w, int revents);
static int create_and_bind(const char *addr, const char *port);
static void client_recv_cb(EV_P_ ev_io *w, int revents);
static void client_send_cb(EV_P_ ev_io *w, int revents);

// Socket Utilites
static int setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void clean_socks5_client(EV_P_ struct socks5_client* client)
{
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

void clean_socks5_server(struct socks5_server* server)
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
    LOGI("Accept");
    struct socks5_server *server = (struct socks5_server *)w;
    int client_fd = accept(server->fd, NULL, NULL);
    if (client_fd == -1) {
        ERROR("accept");
        exit(-1);
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
    struct socks5_client *client = (struct socks5_client *)w;

    ssize_t r = recv(client->fd, client->buf, BUF_SIZE, 0);

    if(r == 0){
        // Connection is going to close
        return;
    }
    else if(r < 0){
        // Error occur
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data
            
            // continue to wait for recv
            return;
        } else {
            ERROR("client_recv_io");
            // Close
            return;
        }
    }


    if(client->stage == 0){
        struct method_select_request *request = (struct method_select_request*)request;
        // check version
        if(request->ver != 0x05){
            // Unknown version
            LOGE("Unknown method: %x", request->ver);
            clean_socks5_client(EV_A_ client);
            return;
        }

        // Send authentication mode
        struct method_select_response response;
        response.ver = SVERSION;
        response.method = 0;
        char *send_buf = (char *)&response;
        send(client->fd, send_buf, sizeof(response), 0);

        client->stage = 1;
    }
    else if(client->stage == 1){
        struct socks5_request *request = (struct socks5_request *)client->buf;
        char host[256], port[16];
        char ss_addr_to_send[320];

        ssize_t addr_len = 0;
        ss_addr_to_send[addr_len++] = request->atyp;

        if(request->cmd == 0x01){
            // Get remote address and port
            if(request->atyp == IPV4){
                // IP V4
                size_t in_addr_len = sizeof(struct in_addr);
                memcpy(ss_addr_to_send + addr_len, client->buf + 4, in_addr_len +
                       2);
                addr_len += in_addr_len + 2;

                if (verbose) {
                    uint16_t p =
                        ntohs(*(uint16_t *)(client->buf + 4 + in_addr_len));
                    sprintf(host, "%s", client->buf + 4);
                    //dns_ntop(AF_INET, (const void *)(buf + 4),
                    //         host, INET_ADDRSTRLEN);
                    sprintf(port, "%d", p);
                }
            }else if(request->atyp == IPV6){
                size_t in6_addr_len = sizeof(struct in6_addr);
                memcpy(ss_addr_to_send + addr_len, client->buf + 4, in6_addr_len +
                       2);
                addr_len += in6_addr_len + 2;

                if (verbose) {
                    uint16_t p =
                        ntohs(*(uint16_t *)(client->buf + 4 + in6_addr_len));

                    sprintf(host, "%s", client->buf + 4);
                    //dns_ntop(AF_INET6, (const void *)(buf + 4),
                    //         host, INET6_ADDRSTRLEN);
                    sprintf(port, "%d", p);
                }

            }else if(request->atyp == DOMAIN){
                // Domain name
                uint8_t name_len = *(uint8_t *)(client->buf + 4);
                ss_addr_to_send[addr_len++] = name_len;
                memcpy(ss_addr_to_send + addr_len, client->buf + 4 + 1, name_len +
                       2);
                addr_len += name_len + 2;

                if (verbose) {
                    uint16_t p =
                        ntohs(*(uint16_t *)(client->buf + 4 + 1 + name_len));
                    memcpy(host, client->buf + 4 + 1, name_len);
                    host[name_len] = '\0';
                    sprintf(port, "%d", p);
                }
            }else{
                LOGE("Unsupport IP protocol: %x", request->atyp);
                clean_socks5_client(EV_A_ client);
                return;
            }

            client->stage = 5;

            if (verbose) {
                LOGI("connect to %s:%s", host, port);
            }
            
        }
        else if(request->cmd == 0x03){
            // UDP ASSOCIATE X'03
            LOGW("UDP ASSOCIATE");
            clean_socks5_client(EV_A_ client);
            return;
        }else{
           LOGE("unsupported cmd: %d", request->cmd);
            struct socks5_response response;
            response.ver = SVERSION;
            response.rep = CMD_NOT_SUPPORTED;
            response.rsv = 0;
            response.atyp = 1;
            char *send_buf = (char *)&response;
            send(client->fd, send_buf, sizeof(struct socks5_response), 0);
            clean_socks5_client(EV_A_ client);
            return; 
        }
    }

    if(client_recv_handler != NULL){
        (*client_recv_handler)(client, revents);
    }
}

static void client_send_cb(EV_P_ ev_io *w, int revents)
{
    
}
