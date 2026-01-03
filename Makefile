CC = gcc
CFLAGS = `pkg-config --cflags gtk4 gtk4-layer-shell-0`
LIBS = `pkg-config --libs gtk4 gtk4-layer-shell-0`
TARGET = hyprwave
SRC = main.c layout.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
