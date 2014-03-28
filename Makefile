
# CC=gcc
CC=clang
OPTFLAGS=-ffast-math -fno-finite-math-only -O3 -march=native
CFLAGS=-fno-strict-aliasing -std=c11 -Wall -pipe -Isrc/ -D_BSD_SOURCE -g
LDFLAGS=-lm -lc -lpthread -lSDL -lGL

eu: Makefile src/*.c src/*.h
	$(CC) $(CFLAGS) $(OPTFLAGS) src/*.c $(LDFLAGS) -o eu
