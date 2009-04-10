CC := $(shell which gcc)

LIBS := fuse 


SRC := src/mossofs.c \
       src/salloc.c \
       src/simple_curl.c \
       src/mosso.c

CFLAGS := $(shell pkg-config --cflags $(LIBS)) \
		  $(shell curl-config --cflags) \
		  -DFUSE_USE_VERSION=22

LDFLAGS := $(shell pkg-config --libs $(LIBS)) \
		   $(shell curl-config --libs)

all: mossofs

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

mossofs: $(patsubst %.c, %.o, $(SRC))
	$(CC) -o $@ $(LDFLAGS) $^

.PHONY: clean
clean:
	-rm src/*.o
	-rm mossofs
