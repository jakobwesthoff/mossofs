CC := $(shell which gcc)

LIBS := fuse \
		libcurl


SRC := src/mossofs.c

CFLAGS := $(shell pkg-config --cflags $(LIBS)) \
		  -DFUSE_USE_VERSION=22

LDFLAGS := $(shell pkg-config --libs $(LIBS))

all: mossofs

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

mossofs: $(patsubst %.c, %.o, $(SRC))
	$(CC) -o $@ $(LDFLAGS) $^

.PHONY: clean
clean:
	-rm src/*.o
	-rm mossofs
