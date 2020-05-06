GCC=gcc
CFLAGS=-std=c99
LFLAGS=

WRK_DIR=$(abspath .)
OBJ_DIR=$(WRK_DIR)/build

.PHONY: all clean

all: recv send

recv: receiver.o helper.o gbn3.o
	$(GCC) -o $@ receiver.o helper.o gbn3.o $(LFLAGS)

send: sender.o helper.o gbn3.o
	$(GCC) -o $@ receiver.o helper.o gbn3.o $(LFLAGS)

%.o: %.c
	$(GCC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f recv send *.o