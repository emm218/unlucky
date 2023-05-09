CC=clang
CFLAGS+=-Wall -Wextra -Werror
LDLIBS+=

debug: CFLAGS+=-g
debug: unlucky

release: CFLAGS+=-O2
release: unlucky

unlucky: instructions.o

clean:
	rm -f *.o unlucky

.PHONY: release clean
