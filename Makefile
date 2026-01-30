CFLAGS += -O2 -march=native -fanalyzer -Wno-unknown-argument -Wpedantic -pedantic -Wall -Wextra -Wuninitialized -Wshadow -Warray-bounds -Wnull-dereference -Wformat -Wunused -Wwrite-strings
PROG = cfan
PREFIX = /usr/local

all: $(PROG)

cfan: config.h cpu.generated.h fans.generated.h $(PROG).c
	$(CC) -o $@ $(PROG).c $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

config.h:
	cp config.def.h $@

clean:
	rm -f $(PROG)

install: $(PROG)
	strip $(PROG)
	chmod 755 $^
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	command -v rsync >/dev/null && rsync -a -r -c $^ $(DESTDIR)$(PREFIX)/bin || cp -f $^ $(DESTDIR)$(PREFIX)/bin

cpu.generated.h:
	./getcpufile > $@

fans.generated.h:
	./getpwmfiles > $@

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)
