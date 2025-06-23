CC = clang
CFLAGS = -Wall -Wextra -O1
LDFLAGS = -lraylib -lm 
TARGET = life_raylib
SRC = life_raylib.c world.c simulation.c draw.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
