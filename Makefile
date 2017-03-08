
DEFINES="-DALLOW_SURROGATES=0"

all:
	gcc -fPIC -Wall -O3 -msse4.1 -Isrc $(DEFINES) -c src/utf8-sse4.c -o src/utf8-sse4.o
	gcc -fPIC -Wall -O3 -mavx2 -Isrc $(DEFINES) -c src/utf8-avx.c -o src/utf8-avx.o
