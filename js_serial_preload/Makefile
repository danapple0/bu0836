PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man
INSTALL ?= install

CFLAGS ?= -mtune=native -pipe -O3

ifeq ($(MAKECMDGOALS),debug)
CFLAGS += -DDEBUG -g -O0
endif

all: js_serial_preload

test: js_serial_preload
	LD_PRELOAD=js_serial_preload.so js_demo

debug: clean all

fg: js_serial_preload
	LD_PRELOAD=js_serial_preload.so /usr/local/bin/fgfs --config=$$HOME/.fgfs/preferences.xml

js_serial_preload: Makefile js_serial_preload.o
	gcc $(CFLAGS) -rdynamic -shared -o js_serial_preload.so js_serial_preload.o -ldl

js_serial_preload.o: Makefile js_serial_preload.c
	gcc $(CFLAGS) -std=c99 -Wall -fPIC -D_GNU_SOURCE -c js_serial_preload.c

check: js_serial_preload
	cppcheck -q -f --enable=all .

vg:
	LD_PRELOAD=js_serial_preload.so valgrind --tool=memcheck --leak-check=full js_demo

clean:
	rm -f js_serial_preload.so js_serial_preload.o core.js_serial_preload.*

install:
	$(INSTALL) --strip --mode 755 js_serial_preload.so $(DESTDIR)$(PREFIX)/lib
