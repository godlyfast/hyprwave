CC = gcc
CFLAGS = `pkg-config --cflags gtk4 gtk4-layer-shell-0`
LIBS = `pkg-config --libs gtk4 gtk4-layer-shell-0`
TARGET = hyprwave
SRC = main.c layout.c paths.c notification.c art.c

# Installation paths
PREFIX ?= $(HOME)/.local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/hyprwave

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Installing HyprWave to $(PREFIX)..."
	install -Dm755 $(TARGET) $(BINDIR)/$(TARGET)
	cp hyprwave-toggle.sh $(BINDIR)/hyprwave-toggle
	chmod +x $(BINDIR)/hyprwave-toggle
	install -Dm644 style.css $(DATADIR)/style.css
	@mkdir -p $(DATADIR)/icons
	install -m644 icons/play.svg $(DATADIR)/icons/
	install -m644 icons/pause.svg $(DATADIR)/icons/
	install -m644 icons/next.svg $(DATADIR)/icons/
	install -m644 icons/previous.svg $(DATADIR)/icons/
	install -m644 icons/arrow-up.svg $(DATADIR)/icons/
	install -m644 icons/arrow-down.svg $(DATADIR)/icons/
	install -m644 icons/arrow-left.svg $(DATADIR)/icons/
	install -m644 icons/arrow-right.svg $(DATADIR)/icons/
	@echo "Installation complete!"
	@echo "Files installed to: $(DATADIR)"
	@echo "Binary installed to: $(BINDIR)/$(TARGET)"
	@echo ""
	@echo "Run 'hyprwave' to start"

uninstall:
	@echo "Uninstalling HyprWave..."
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(BINDIR)/hyprwave-toggle
	rm -rf $(DATADIR)
	@echo "Uninstall complete!"

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean install uninstall run
