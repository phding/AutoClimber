

#include "socks5.h"

// a single header file is required
#include <ev.h>





// Socks5 handler

static void stdin_cb (EV_P_ ev_io *w, int revents)
{
    // for one-shot events, one must manually stop the watcher
    // with its corresponding stop function.
    ev_io_stop (EV_A_ w);

    // this causes all nested ev_run's to stop iterating
    ev_break (EV_A_ EVBREAK_ALL);
}


void create_socks5_server(const char *addr, const char *port)
{
	ev_io stdin_watcher;
	
	ev_io_init (&stdin_watcher, stdin_cb, /*STDIN_FILENO*/ 0, EV_READ);

	return;
}

