#include <error.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h> // close function
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "socks5_proxy.h"
#include "logging.h"



static void client_recv_io(EV_P_ struct socks5_client *client, struct socks5_request* request);
int create_remote_connection(char* host, char* port);

struct proxy_node* create_proxy_node()
{
	// Create socks5 server
	struct proxy_node *node;
	node = malloc(sizeof(struct proxy_node));
	memset(node, 0, sizeof(struct proxy_node));
	return node;
}

static void client_recv_io(EV_P_ struct socks5_client *client, struct socks5_request* request)
{

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
        response.rep = RES_CMD_NOT_SUPPORTED;
        response.rsv = 0;
        response.atyp = 1;
        char *send_buf = (char *)&response;
        send(client->fd, send_buf, sizeof(struct socks5_response), 0);
        clean_socks5_client(EV_A_ client);
        return; 
    }
    
    if (verbose) {
        LOGI("connect to %s:%s", host, port);
    }
	// Check request type

	if(client->ptr == NULL){
		client->ptr = (void*)create_proxy_node();
	}

	struct proxy_node* node = (struct proxy_node*)client->ptr;

	if(!node->connected){

		// TODO: check whether go direct or via another proxy

		// Not connected yet, connecting directly to remote
        create_remote_connection(host, port);
    }
}

int create_remote_connection(char* host, char* port)
{
    int ret, listen_sock;
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */

    ret = getaddrinfo(host, port, &hints, &result);
    if(ret != 0){
        LOGI("getaddrinfo: %s", gai_strerror(ret));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {

        LOGI("Host = %s", rp->ai_addr->sa_data);
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_sock == -1) {
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #ifdef SO_NOSIGPIPE
        setsockopt(listen_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
        #endif

        ret = bind(listen_sock, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            /* We managed to bind successfully! */
            break;
        } else {
            ERROR("bind");
        }

        close(listen_sock);
    }

    
    LOGI("Success");
    return 0;
}

void init_socks5_proxy()
{
	client_recv_handler = &client_recv_io;
}