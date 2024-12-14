CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2
TARGET = event_loop
SRC = main.c

build:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: build
	./$(TARGET)

clean:
	rm -f $(TARGET)
