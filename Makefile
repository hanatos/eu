
# CC=gcc
CC=clang
OPTFLAGS=-ffast-math -fno-finite-math-only -O3 -march=native
CFLAGS=-fno-strict-aliasing -std=c11 -Wall -pipe -Isrc/ -D_BSD_SOURCE -g
LDFLAGS=-lm -lc -lpthread -lSDL -lGL
PREFIX=/usr

# configure the color module: custom will use this.
COLOR=srgb
include config.mk

.PHONY: clean all

all: eu

debug: OPTFLAGS=-ggdb3
debug: eu

eu: Makefile src/*.c src/*.h
	cp -f color/$(COLOR).h src/colorout_custom.h
	$(CC) $(CFLAGS) $(OPTFLAGS) src/*.c $(LDFLAGS) -o eu

install: eu
	cp -f eu ${PREFIX}/bin/
	cp -f eu.complete /etc/bash_completion.d/eu
	gzip -c man/eu.man > ${PREFIX}/share/man/man1/eu.1.gz

clean:
	rm -f eu
