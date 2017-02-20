# benjamin Maitland
CC=gcc
CFLAGS=-std=c99 -c -Wall -Wextra -g -O3 -flto -march=native
LIBS=-lSDL2 -lm -pthread libthreadpool.a
OBJECTS=MySim.o DiamondSquare.o xoroshiro128plus.o utilities.o interpolation.o sobel.o map2d.o erosion.o tectonics.o

all: main

main: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o MySim

clean:
	rm -rf *.o
