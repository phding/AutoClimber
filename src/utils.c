
#include <stdlib.h>
#include <arpa/inet.h>

#include "utils.h"
#include "socks5.h"
#include "config.h"
#include "logging.h"


bool get_host_addr(char* host, char* port, struct addrinfo* addr, int* addr_host_len)
{
	int ret, addr_len;
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

        memcpy(addr, rp->ai_addr, addr_len);
        *addr_host_len = addr_len;
        freeaddrinfo(result);
        return true;
    }

    LOGW("Unable to find proper address for the host %s", host);
    freeaddrinfo(result);
    return false;
}