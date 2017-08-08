# benjamin Maitland
CC=gcc
#GL_LIBS = -lGLEW -lglfw3 -lGL -lX11 -lXxf86vm -lpthread -lXrandr -lXinerama -lXcursor -lXi -ldl -lm
CFLAGS=-std=c99 -c -Wall -Wextra -g  -march=native
# -O3 -flto
LIBS=-lSDL2 -lm -pthread $(GL_LIBS)
OBJECTS=MySim.o DiamondSquare.o xoroshiro128plus.o utilities.o interpolation.o sobel.o map2d.o erosion.o tectonics.o weather.o threadpool.o

all: main

main: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o MySim

clean:
	rm -rf *.o
