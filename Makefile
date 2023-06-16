CC=clang
CFLAGS+=-Wall -Wextra -Werror
LDLIBS+=

debug: CFLAGS+=-g
debug: unlucky

release: CFLAGS+=-O2
release: unlucky

unlucky: instructions.o

.depend/%.d: %.c
	@mkdir -p $(dir $@) 
	$(CC) $(CFLAGS) -MM $^ -MF $@

include $(patsubst %.c, .depend/%.d, $(wildcard *.c))

clean:
	rm -f *.o unlucky

.PHONY: release clean
