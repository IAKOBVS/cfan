.POSIX:

# NVML (uncomment to disable)
LIB_CUDA = /opt/cuda/lib64
# LDFLAGS_CUDA = -L$(LIB_CUDA) -lnvidia-ml
# REQ_CUDA = gpu.generated.h

LDFLAGS += $(LDFLAGS_CUDA)
REQ += $(REQ_CUDA)

CFLAGS = -O2 -march=native -fanalyzer -Wno-unknown-warning-option -Warray-bounds -Wnull-dereference -Wformat -Wunused -Wwrite-strings
PROG = cfan
PREFIX = /usr/local

all: $(PROG)

cfan: config.h cpu.generated.h table-fans.generated.h $(PROG).c $(REQ)
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

table-fans.generated.h:
	./getpwmfiles > $@


# Comment out parts of the config.h and the Makefile
enable_cuda: $(config) $(disable-nvml)
	@mv config.h config.h.bak
	@ # Uncomment out USE_CUDA line
	@sed 's/^[ \t]*\(#.*define.*USE_CUDA.*1\).*/\/\* \1 \*\//' config.h.bak > config.h
	@rm config.h.bak
	@cp Makefile Makefile.bak
	@ # Uncomment out LDFLAGS_CUDA line
	@sed 's/^#[ \t]*\(LDFLAGS_CUDA.*\)/\1/; s/^#[ \t]*\(REQ_CUDA.*\)/\1/' Makefile.bak > Makefile
	@rm Makefile.bak

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)
