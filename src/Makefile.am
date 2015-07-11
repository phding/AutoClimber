# Global CFlags

AM_CFLAGS = -g -O2 -Wall -Werror -Wno-deprecated-declarations -fno-strict-aliasing -std=gnu99 -D_GNU_SOURCE
AM_CFLAGS += -I$(top_srcdir)/libev

AM_LDFLAGS = -static

# Target

bin_PROGRAMS = autoclimber

# Source

autoclimber_SOURCES = autoclimber.c \
						socks5.c

# Link

autoclimber_LDADD = $(top_builddir)/libev/libev.la \
				 $(INET_NTOP_LIB)
autoclimber_CFLAGS = $(AM_CFLAGS) -DUDPRELAY_LOCAL