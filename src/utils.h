#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>

bool get_host_addr(char* host, char* port, struct addrinfo* addr, int* addr_len);