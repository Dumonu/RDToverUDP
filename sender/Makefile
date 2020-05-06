GCC=gcc
CFLAGS=-std=c99
LFLAGS=

WRK_DIR=$(abspath .)
OBJ_DIR=$(WRK_DIR)/build

EXE=send
OBJ=$(addsuffix .o,$(basename $(wildcard *.c)))

.PHONY: all debug release clean

all: debug release

debug: $(EXE).dbg

release: $(EXE)

$(EXE): $(addprefix $(OBJ_DIR)/,$(OBJ))
	$(GCC) -o $@ $^ $(LFLAGS)

$(EXE).dbg: $(addprefix $(OBJ_DIR).dbg/,$(OBJ))
	$(GCC) -o $@ $^ $(LFLAGS)

$(OBJ_DIR)/%.o: %.c $(OBJ_DIR)
	$(GCC) -c $(CFLAGS) -o $@ $<

$(OBJ_DIR).dbg/%.o: %.c $(OBJ_DIR).dbg
	$(GCC) -c $(CFLAGS) -g -DDEBUG_ -o $@ $<

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR).dbg:
	mkdir $(OBJ_DIR).dbg

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(OBJ_DIR).dbg
	rm -f $(EXE)
	rm -f $(EXE).dbg