PATH  := $(PATH):$(PWD)/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin
SHELL := env PATH=$(PATH) /bin/bash
TARGET = imperium
LIBS = -lm
CC = arm-none-linux-gnueabihf-gcc
CFLAGS = -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
