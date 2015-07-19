#include "socks5_proxy.h"
#include "logging.h"


static void client_recv_io(struct socks5_client *client, int revents);

static void client_recv_io(struct socks5_client *client, int revents)
{
	LOGI("Receive");
}

void init_socks5_proxy()
{
	client_recv_handler = &client_recv_io;
}