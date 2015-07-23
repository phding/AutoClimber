
#include "socks5.h"
#include "socks5_proxy.h"
#include "logging.h"

// Library
#include <stdio.h>


int main(int argc, char** argv)
{
	const char* addr = "10.62.166.18";
	const char* port = "3000";
	LOGI("Try to bind %s:%s", addr, port);
	struct socks5_server* server = create_socks5_server(addr, port);

	init_socks5_proxy();

	// Run 
	ev_run(server->loop, 0);

    LOGI("Sock5 closed");
    ev_io_stop(server->loop, &server->io);

}