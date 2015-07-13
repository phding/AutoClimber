
#include "socks5.h"

// Library
#include <stdio.h>


int main(int argc, char** argv)
{
	struct socks5_server* server = create_socks5_server("tohold.cloudapp.net", "3000");

	// Run 
	ev_run(server->loop, 0);
}