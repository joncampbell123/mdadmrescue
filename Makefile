all: mdadmrescue

CFLAGS=-g3 -O0 -fno-omit-frame-pointer -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall `pkg-config fuse --cflags` -pedantic -fPIC -std=gnu99
LDFLAGS=-lz -lm -shared
PWD=`pwd`

mdadmrescue: mdadmrescue.c
	gcc -pedantic -std=c99 $(CFLAGS) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -o mdadmrescue mdadmrescue.c `pkg-config fuse --libs` `pkg-config fuse --cflags` -lpthread -lisp-utils-text -lm -lbz2 -lz -llzma -lhashish

clean:
	rm -f mdadmrescue *.o

install:

