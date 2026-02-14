# archiver-bot - suckless web archiver
# See LICENSE file for copyright and license details.

VERSION = 0.2.0

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
SRC = archiver.c crawl.c fetch.c parse.c robots.c util.c
OBJ = $(SRC:.c=.o)

all: archiver-bot

.c.o:
	$(CC) $(CFLAGS) -c $<

archiver-bot: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f archiver-bot $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f archiver-bot $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/archiver-bot

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/archiver-bot

.PHONY: all clean install uninstall
