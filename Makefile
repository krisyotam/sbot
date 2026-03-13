# sbot - Simple Archiver Bot
# See LICENSE file for copyright and license details.

VERSION = 0.3.0

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# includes and libs
INCS = `pkg-config --cflags libcurl`
LIBS = `pkg-config --libs libcurl`

# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Os $(INCS) $(CPPFLAGS)
LDFLAGS  = $(LIBS)

# compiler
CC = cc

# sources
SRC = archiver.c crawl.c detect.c fetch.c parse.c robots.c util.c
OBJ = $(SRC:.c=.o)

all: sbot

.c.o:
	$(CC) $(CFLAGS) -c $<

sbot: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f sbot $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f sbot $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/sbot

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sbot

.PHONY: all clean install uninstall
