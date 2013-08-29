CC=gcc
OBJECTS=cpu.o mmu.o cart.o display.o serial.o main.o timer.o audio.o input.o
CFLAGS=-O0 -g -Wall -DMACOSX
EXECUTABLES=gb

all: gb

gb: $(OBJECTS)
	gcc $(LDFLAGS) -o gb $^

clean:
	-rm -f *.o $(EXECUTABLES)

.PHONY: clean tests
