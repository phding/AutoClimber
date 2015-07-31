#include <error.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h> // close function
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "socks5_proxy.h"
#include "logging.h"


static void client_send_data(EV_P_ struct socks5_client *client);
static void client_recv_data(EV_P_ struct socks5_client *client);
static void client_recv_request(EV_P_ struct socks5_client *client, struct socks5_request* request);
static struct proxy_remote_client * create_remote(int fd);
static void remote_recv_cb(EV_P_ ev_io *w, int revents);
static void remote_send_cb(EV_P_ ev_io *w, int revents);
static void remote_timeout_cb(EV_P_ ev_timer *watcher, int revents);
static struct proxy_remote_client* create_remote(int fd);
static void close_and_free_remote(EV_P_ struct proxy_remote_client *remote);
static void send_socks5_response(int fd, int rep);


struct proxy_remote_client* create_direct_remote_connection(char* host, char* port);


struct proxy_node* create_proxy_node()
{
	// Create socks5 server
	struct proxy_node *node;
	node = malloc(sizeof(struct proxy_node));
	memset(node, 0, sizeof(struct proxy_node));
	return node;
}

static void client_recv_data(EV_P_ struct socks5_client *client)
{
    int ret;
    struct proxy_node* proxy_node;
    struct proxy_remote_client* remote;

    //LOGI("Receiving from client");

    if(client->ptr != NULL){
        proxy_node = (struct proxy_node*)client->ptr;
        remote = proxy_node->remote_client;
        if(proxy_node->connected == true){
            // send to remote
            ret = send(remote->fd, client->buf + client->buf_offset, client->buf_len, 0);
            if(ret < 0){
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ERROR("client_recv_data");
                    // close and free
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_socks5_client(EV_A_ client);
                }
                return;
            }
            else if(ret < client->buf_len){
                client->buf_len -= ret;
                client->buf_offset += ret;

                // congestion
                ev_io_stop(EV_A_ & client->recv_handler.io);
                ev_io_start(EV_A_ & remote->send_handler.io);
                ev_timer_start(EV_A_ & remote->send_handler.watcher);
            }
        }else{
            LOGW("Not connected yet");
        }
    }else{
        LOGE("No such proxy node was found");
    }
}

static void client_recv_request(EV_P_ struct socks5_client *client, struct socks5_request* request)
{
    char host[256], port[16];
    char ss_addr_to_send[320];

    ssize_t addr_len = 0;
    ss_addr_to_send[addr_len++] = request->atyp;
    struct proxy_node* proxy_node;
    struct proxy_remote_client* remote;


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
            close_and_free_socks5_client(EV_A_ client);
            return;
        }

    }
    else if(request->cmd == 0x03){
        // UDP ASSOCIATE X'03
        LOGW("UDP ASSOCIATE");
        close_and_free_socks5_client(EV_A_ client);
        return;
    }else{
        LOGE("unsupported cmd: %d", request->cmd);
        send_socks5_response(client->fd, RES_CMD_NOT_SUPPORTED);
        close_and_free_socks5_client(EV_A_ client);
        return; 
    }
    
    if (verbose) {
        LOGI("Attempting connection to %s:%s", host, port);
    }
	// Check request type
    proxy_node = create_proxy_node();
    client->ptr = (void*)proxy_node;
    proxy_node->socks5_client = client;


    // TODO: check whether go direct or via another proxy

    remote = create_direct_remote_connection(host, port);
    if(remote == NULL){
        // Unable to create direct remote connection
        LOGW("Unable to create direct connection");

        send_socks5_response(client->fd, RES_NETWORK_UNREACHABLE);
        close_and_free_socks5_client(EV_A_ client);
        return;
    }

    proxy_node->remote_client = remote;
    remote->node = proxy_node;
    proxy_node->direct = true;
    // Connect to remote
    connect(remote->fd, (struct sockaddr *)&(remote->addr), remote->addr_len);
    LOGI("Start");

    // Now listen to remote
    ev_io_start(EV_A_ & remote->recv_handler.io);

    // wait on remote connected event
    ev_timer_start(EV_A_ & remote->send_handler.watcher);
    ev_io_start(EV_A_ & remote->send_handler.io);

}

static void remote_recv_cb(EV_P_ ev_io *w, int revents)
{
    int ret;
    struct proxy_io_handler* io_handler = (struct proxy_io_handler*)w;
    struct proxy_remote_client* remote = io_handler->remote;
    struct proxy_node* node = remote->node;
    struct socks5_client* client = node->socks5_client;

    ev_timer_stop(EV_A_ & remote->recv_handler.watcher);
    
    //LOGI("Receiving from remote");

    // Receive
    remote->buf_len = recv(remote->fd, remote->buf, BUF_SIZE, 0);
    if(remote->buf_len == 0){
        // Connection is going to close
        close_and_free_remote(EV_A_ remote);
        close_and_free_socks5_client(EV_A_ client);
        return;
    }
    else if(remote->buf_len < 0){
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ERROR("remote_recv_cb");
            // close and free
            close_and_free_remote(EV_A_ remote);
            close_and_free_socks5_client(EV_A_ client);
        }
        return;
    }else {
        // Send
        ret = send(client->fd, remote->buf + remote->buf_offset, remote->buf_len, 0);
        if(ret < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no data
                // continue to wait for recv
                return;
            } else {
                ERROR("failed to redirect package to client");
                close_and_free_remote(EV_A_ remote);
                close_and_free_socks5_client(EV_A_ client);
                // Close
                return;
            }
        }
        else if(ret < remote->buf_len){
            remote->buf_len -= ret;
            remote->buf_offset += ret;

            // congestion
            ev_io_stop(EV_A_ & remote->recv_handler.io);
            ev_io_start(EV_A_ & client->send_handler.io);
        }
    }
}

static void client_send_data(EV_P_ struct socks5_client *client)
{
    int ret;
    struct proxy_node* proxy_node;
    struct proxy_remote_client* remote;

    //LOGI("Send to client");

    if(client->ptr != NULL){

        proxy_node = (struct proxy_node*)client->ptr;
        remote = proxy_node->remote_client;
        if(proxy_node->connected == true){

            if(remote->buf_len == 0){
                // close and free
                close_and_free_remote(EV_A_ remote);
                close_and_free_socks5_client(EV_A_ client);
            }else{
                ret = send(client->fd, remote->buf + remote->buf_offset, remote->buf_len, 0);
                if(ret < 0){
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        ERROR("client_send_data");
                        // close and free
                        close_and_free_remote(EV_A_ remote);
                        close_and_free_socks5_client(EV_A_ client);
                    }
                    return;
                }else if(ret < remote->buf_len){

                    // partly sent, move memory, wait for the next time to send
                    remote->buf_len -= ret;
                    remote->buf_offset += ret;
                    return;
                }else{
                    remote->buf_len = 0;
                    remote->buf_offset = 0;
                    ev_io_stop(EV_A_ & client->send_handler.io);
                    ev_io_start(EV_A_ & remote->recv_handler.io);
                }
            }
        }else{
            LOGW("Not connected yet");
            return;
        }

    }else{
        LOGE("No such proxy node was found");
        return;
    }
}

static void remote_send_cb(EV_P_ ev_io *w, int revents)
{
    int ret;
    struct proxy_io_handler* io_handler = (struct proxy_io_handler*)w;
    struct proxy_remote_client* remote = io_handler->remote;
    struct proxy_node* node = remote->node;
    struct socks5_client* client = node->socks5_client;
    ev_timer_stop(EV_A_ & remote->send_handler.watcher);

    if (!node->connected) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof addr;
        int r = getpeername(remote->fd, (struct sockaddr *)&addr, &len);
        if (r == 0) {
            LOGI("Connected");
            node->connected = true;
            ev_io_stop(EV_A_ & remote->send_handler.io);

            // Send successful
            //send_socks5_response(node->socks5_client->fd, RES_SUCCEEDED);
            struct sockaddr_in sock_addr;
            memset(&sock_addr, 0, sizeof(sock_addr));
            struct socks5_response response;
            response.ver = SVERSION;
            response.rep = RES_SUCCEEDED;
            response.rsv = 0;
            response.atyp = 1;

            memcpy(client->buf, &response, sizeof(struct socks5_response));
            memcpy(client->buf + sizeof(struct socks5_response),
                   &sock_addr.sin_addr, sizeof(sock_addr.sin_addr));
            memcpy(client->buf + sizeof(struct socks5_response) +
                   sizeof(sock_addr.sin_addr),
                   &sock_addr.sin_port, sizeof(sock_addr.sin_port));
            int reply_size = sizeof(struct socks5_response) +
                             sizeof(sock_addr.sin_addr) +
                             sizeof(sock_addr.sin_port);
            int ret = send(client->fd, client->buf, reply_size, 0);
            if(ret < reply_size){
                LOGE("failed to send fake reply");
                close_and_free_remote(EV_A_ remote);
                close_and_free_socks5_client(EV_A_ client);
                return;
            }

            ev_io_start(EV_A_ & client->recv_handler.io);
            ev_io_stop(EV_A_ & remote->send_handler.io);
            return;

        } else {
            // not connected
            ERROR("getpeername");
            send_socks5_response(node->socks5_client->fd, RES_CONNECTION_REFUSED);
            close_and_free_remote(EV_A_ remote);
            close_and_free_socks5_client(EV_A_ client);
            return;
        }
    }else if(client->buf_len == 0){
        // close and free
        close_and_free_remote(EV_A_ remote);
        close_and_free_socks5_client(EV_A_ client);
    }else{
        // has something to send
        ret = send(remote->fd, client->buf + client->buf_offset, client->buf_len, 0);
        if(ret < 0){
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ERROR("remote_send_cb");
                // close and free
                close_and_free_remote(EV_A_ remote);
                close_and_free_socks5_client(EV_A_ client);
            }
            return;
        }else if(ret < client->buf_len){
            // partly sent, move memory, wait for the next time to send
            client->buf_len -= ret;
            client->buf_offset += ret;
            return;
        }else{
            client->buf_len = 0;
            client->buf_offset = 0;
            ev_timer_start(EV_A_ & remote->recv_handler.watcher);
            ev_io_start(EV_A_ & client->recv_handler.io);
            ev_io_stop(EV_A_ & remote->send_handler.io);
        }
    }
}

static void remote_timeout_cb(EV_P_ ev_timer *watcher, int revents)
{
    LOGI("Timeout");
    struct proxy_io_handler *io_handler = (struct proxy_io_handler *)(((void *)watcher)- sizeof(ev_io));
    struct proxy_remote_client *remote = (struct proxy_remote_client *)io_handler->remote;
    struct proxy_node* node = remote->node;
    if (verbose) {
        LOGI("TCP connection timeout");
    }

    //return
    send_socks5_response(node->socks5_client->fd, RES_TTL_EXPIRED);
    close_and_free_remote(EV_A_ remote);
    close_and_free_socks5_client(EV_A_ node->socks5_client);
}

static void send_socks5_response(int fd, int rep)
{
    struct socks5_response response;
    response.ver = SVERSION;
    response.rep = rep;
    response.rsv = 0;
    response.atyp = 1;
    char *send_buf = (char *)&response;
    send(fd, send_buf, sizeof(struct socks5_response), 0);
}

static void close_and_free_remote(EV_P_ struct proxy_remote_client *remote)
{
    LOGI("Close remote");
    if (remote != NULL) {
        ev_timer_stop(EV_A_ & remote->send_handler.watcher);
        ev_io_stop(EV_A_ & remote->send_handler.io);
        ev_io_stop(EV_A_ & remote->recv_handler.io);
        close(remote->fd);
        if(remote->buf != NULL){
            free(remote->buf);
        }
        free(remote);
    }
}

static struct proxy_remote_client* create_remote(int fd)
{
    struct proxy_remote_client *remote;
    remote = malloc(sizeof(struct proxy_remote_client));

    memset(remote, 0, sizeof(struct proxy_remote_client));

    remote->buf = malloc(BUF_SIZE);
    remote->fd = fd;
    ev_io_init(&remote->recv_handler.io, remote_recv_cb, fd, EV_READ);
    ev_io_init(&remote->send_handler.io, remote_send_cb, fd, EV_WRITE);
    ev_timer_init(&remote->send_handler.watcher, remote_timeout_cb, MAX_CONNECT_TIMEOUT, 0);
    ev_timer_init(&remote->recv_handler.watcher, remote_timeout_cb, MAX_CONNECT_TIMEOUT, 0);
    remote->recv_handler.remote = remote;
    remote->send_handler.remote = remote;
    return remote;
}

struct proxy_remote_client* create_direct_remote_connection(char* host, char* port)
{
    int ret, remote_fd, addr_len;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket


    ret = getaddrinfo(host, port, &hints, &result);
    if(ret != 0){
        LOGI("getaddrinfo: %s", gai_strerror(ret));
        return NULL;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        void *addr;
        if (rp->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
            addr_len = INET_ADDRSTRLEN;
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
            addr_len = INET6_ADDRSTRLEN;
        }

        if(verbose){
            inet_ntop(rp->ai_family, addr, ipstr, sizeof ipstr);
            LOGI("Host = %s", ipstr);
        }

        remote_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (remote_fd == -1) {
            continue;
        }

        int opt = 1;
        setsockopt(remote_fd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
        #ifdef SO_NOSIGPIPE
        setsockopt(remote_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
        #endif

        setnonblocking(remote_fd);

        struct proxy_remote_client *remote = create_remote(remote_fd);
        remote->addr_len = addr_len;
        memcpy(&remote->addr, rp->ai_addr, addr_len);
        return remote;
    }

    LOGW("Unable to create socket");
    return NULL;
}

void init_socks5_proxy()
{
	client_recv_data_handler = &client_recv_data;
    client_recv_request_handler = &client_recv_request;
    client_send_data_handler = &client_send_data;
}