# Makefile for Programming Assignment 03 (myshell)
#
#   make          build bin/myshell
#   make clean    delete obj/*.o and bin/myshell
#
# It compiles every .c file in src/ into obj/*.o, then links them into bin/myshell.

CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
LDFLAGS =
SRCS    = $(wildcard src/*.c)
OBJS    = $(patsubst src/%.c, obj/%.o, $(SRCS))
TARGET  = bin/myshell

all: $(TARGET)

# link: all object files -> final program
$(TARGET): $(OBJS) | bin
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# compile: one .c file -> one .o file (rebuilt when the .c or shell.h changes)
obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

# the "| bin" / "| obj" are order-only prerequisites: create the folder if it is missing
bin obj:
	mkdir -p $@

clean:
	rm -f obj/*.o $(TARGET)

.PHONY: all clean
