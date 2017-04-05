
DEFINES="-DALLOW_SURROGATES=0"
CFLAGS=-fPIC -Wall -O3 -std=c99

all:
	gcc $(CFLAGS) -msse4.1 -Isrc $(DEFINES) -c src/utf8-sse4.c -o src/utf8-sse4.o
	gcc $(CFLAGS) -mavx2 -Isrc $(DEFINES) -c src/utf8-avx.c -o src/utf8-avx.o
