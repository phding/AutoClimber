
#include "socks5_client.h"
#include "socks5.h"
#include "socks5_proxy.h"

#include "utils.h"
#include "logging.h"

struct addrinfo socks5_server_addr;
int socsk5_server_addr_len;
/*
static void remote_recv_cb(EV_P_ ev_io *w, int revents);
static void remote_send_cb(EV_P_ ev_io *w, int revents);

struct proxy_remote_client* create_socsk5_remote_connection(char* host, char* port);

static void remote_recv_cb(EV_P_ ev_io *w, int revents)
{

}

static void remote_send_cb(EV_P_ ev_io *w, int revents)
{

}
*/


struct proxy_remote_client* create_socsk5_remote_connection(char* host, char* port)
{
	/*
	// Create socks5 client
	struct method_select_request request;

	request.ver = SVERSION;
	request.nmethods = 1;
	request.methods[0] = AUTH_NO_REQUIRED;
*/
	// Send request
#ifdef TCP_FASTOPEN
#endif

	return NULL;


}


void init_socks5_client(char* host, char* port)
{
	bool ret;
	// Register for create remote
	create_remote_client = &create_socsk5_remote_connection;

	// Get server's addr
	ret = get_host_addr(host, port, &socks5_server_addr, &socsk5_server_addr_len);
	if(!ret){
		FATAL("Can't find addr");
	}

}