# benjamin Maitland
CC=gcc
CFLAGS=-std=c99 -c -Wall -Wextra -g
LIBS=-lSDL2 -lm
OBJECTS=MySim.o DiamondSquare.o xoroshiro128plus.o utilities.o interpolation.o sobel.o

all: main

main: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o MySim

clean:
	rm -rf *.o
