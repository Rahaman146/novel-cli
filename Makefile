# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Libraries
LIBS = -lncurses -lcurl -lcjson

# Installation directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Source files
SRC = main.c ui.c chapter_controller.c controller.c network.c cache.c library.c webnovel.c

# Object files
OBJ = $(SRC:.c=.o)

# Output binary
TARGET = novel-cli

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

# Compile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Install to system
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Installation complete! Run '$(TARGET)' from anywhere."

# Uninstall from system
uninstall:
	@echo "Uninstalling $(TARGET) from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstallation complete."

# Clean build files
clean:
	rm -f $(OBJ) $(TARGET)

# Rebuild everything
re: clean all

# Install dependencies (Arch/Manjaro with yay)
deps-arch:
	yay -S --noconfirm ncurses curl cjson

# Install dependencies (Arch/Manjaro with pacman)
deps-pacman:
	sudo pacman -S --noconfirm ncurses curl cjson

# Install dependencies (Ubuntu/Debian)
deps-debian:
	sudo apt-get update
	sudo apt-get install -y libncurses5-dev libncursesw5-dev libcurl4-openssl-dev libcjson-dev

# Install dependencies (Fedora/RHEL)
deps-fedora:
	sudo dnf install -y ncurses-devel libcurl-devel cjson-devel

# Install dependencies (macOS)
deps-macos:
	brew install ncurses curl cjson

# Help target
help:
	@echo "Available targets:"
	@echo "  make              - Build the application"
	@echo "  make install      - Install to $(BINDIR)"
	@echo "  make uninstall    - Remove from $(BINDIR)"
	@echo "  make clean        - Remove build files"
	@echo "  make re           - Clean and rebuild"
	@echo ""
	@echo "Dependency installation:"
	@echo "  make deps-arch    - Install deps with yay (Arch/Manjaro)"
	@echo "  make deps-pacman  - Install deps with pacman (Arch/Manjaro)"
	@echo "  make deps-debian  - Install deps (Ubuntu/Debian)"
	@echo "  make deps-fedora  - Install deps (Fedora/RHEL)"
	@echo "  make deps-macos   - Install deps (macOS)"
	@echo ""
	@echo "Quick install (recommended):"
	@echo "  chmod +x install.sh && ./install.sh"

.PHONY: all clean re install uninstall deps-arch deps-pacman deps-debian deps-fedora deps-macos help
