# GNU Make + GCC (MSYS2/MinGW). Builds object files separately to avoid some MinGW
# assembler issues when compiling all sources in one command.

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -I include
LDFLAGS :=
SRCS    := src/main.c src/lexer.c src/parse.c src/ast.c src/value.c src/interp.c src/pathutil.c
OBJS    := $(SRCS:src/%.c=build/%.o)

.PHONY: all clean run

all: build/flow$(EXEEXT)

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/flow$(EXEEXT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -rf build

run: build/flow$(EXEEXT)
	./build/flow$(EXEEXT) examples/demo.flow
