#include <netinet/tcp.h>
#include <error.h>

#include "socks5_proxy.h"
#include "logging.h"



static void client_recv_io(struct socks5_client *client, int revents);

struct proxy_node* create_proxy_node()
{
	// Create socks5 server
	struct proxy_node *node;
	node = malloc(sizeof(struct proxy_node));
	memset(node, 0, sizeof(struct proxy_node));
	return node;
}

static void client_recv_io(struct socks5_client *client, int revents)
{
	if(client->ptr == NULL){
		client->ptr = (void*)create_proxy_node();
	}

	struct proxy_node* node = (struct proxy_node*)client->ptr;

	if(!node->connected){

		// TODO: check whether go direct or via another proxy

		// Not connected yet, connecting directly to remote



	}




}

void init_socks5_proxy()
{
	client_recv_handler = &client_recv_io;
}