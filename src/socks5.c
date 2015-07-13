
#include <fcntl.h>
#include <unistd.h> // close function
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/types.h>

#include "socks5.h"
#include "logging.h"


// Function declaration
static struct socks5_client* create_socsk5_client(int fd);
static void accept_cb(EV_P_ ev_io *w, int revents);
static void free_connections(struct ev_loop *loop);
static int create_and_bind(const char *addr, const char *port);

// Socket Utilites
static int setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void free_connections(struct ev_loop *loop)
{
}

void clean_socks5_server(struct socks5_server* server)
{
	// Clean up
    ev_io_stop(server->loop, &server->io);
    free_connections(server->loop);

    free(server->sockaddr);
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
	setnonblocking(listen_fd);
	server->fd = listen_fd;
    LOGI("TCP socsk5 enabled");

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

    client->fd = fd;
    return client;
}



// Accept incoming connection
static void accept_cb(EV_P_ ev_io *w, int revents)
{
    struct socks5_server *server = (struct socks5_server *)w;
    int client_fd = accept(server->fd, NULL, NULL);
    if (client_fd == -1) {
        ERROR("accept");
        return;
    }

    setnonblocking(client_fd);
    int opt = 1;
    setsockopt(client_fd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));

    struct socks5_client *client = create_socsk5_client(client_fd);

    free(client);
}
