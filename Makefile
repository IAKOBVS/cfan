.POSIX:

# NVML (uncomment to disable)
LIB_CUDA = /opt/cuda/lib64
LDFLAGS_CUDA = -L$(LIB_CUDA) -lnvidia-ml
REQ_CUDA = gpu-nvidia.h

LDFLAGS += $(LDFLAGS_CUDA)
REQ += $(REQ_CUDA)

REQ += config.h cpu.generated.h table-fans.generated.h table-temp.h path.h

CFLAGS_DEBUG = -DDEBUG=1 -DEXIT_SLOW=1
CFLAGS = -O2 -march=native -flto -fanalyzer -Wno-unknown-warning-option -Warray-bounds -Wnull-dereference -Wformat -Wunused -Wwrite-strings
PROG = cfan
PREFIX = /usr/local
CC = cc

all: $(PROG)

cfan: $(PROG).c $(REQ)
	$(CC) -o $@ $(PROG).c $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

$(PROG)-debug: $(PROG).c $(REQ)
	$(CC) -o $@ $(PROG).c $(CFLAGS_DEBUG) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

debug: $(PROG)-debug

config.h:
	cp config.def.h $@

table-temp.h:
	cp table-temp.def.h $@

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

config:
	@echo 'Automated configuration:'
	@echo 'Usage: make [OPTION]...'
	@echo ''
	@echo 'enable-cuda'
	@echo ''
	@echo 'For example, to enable CUDA, run:'
	@echo 'make enable-cuda'

# Comment out parts of the config.h and the Makefile
enable_cuda: $(config)
	@mv config.h config.h.bak
	@ # Uncomment out USE_CUDA line
	@sed 's/^.*\(#.*define USE_CUDA 1\).*/\1/' config.h.bak > config.h
	@rm config.h.bak
	@cp Makefile Makefile.bak
	@ # Uncomment out LDFLAGS_CUDA line
	@sed 's/^#.*\(LDFLAGS_CUDA.*\)/\1/; s/^#.*\(REQ_CUDA.*\)/\1/' Makefile.bak > Makefile
	@rm Makefile.bak

disable_cuda: $(config)
	@mv config.h config.h.bak
	@ # Uncomment out USE_CUDA line
	@sed 's/^\(#.*define.*USE_CUDA.*1\).*/\/\* \1 \*\//' config.h.bak > config.h
	@rm config.h.bak
	@cp Makefile Makefile.bak
	@ # Uncomment out LDFLAGS_CUDA line
	@sed 's/^\(LDFLAGS_CUDA.*\)/# \1/; s/^\(REQ_CUDA.*\)/# \1/' Makefile.bak > Makefile
	@rm Makefile.bak

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)
