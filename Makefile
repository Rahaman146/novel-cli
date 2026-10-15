# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -Isrc

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    TARGET = novel-cli.exe
    CFLAGS += -DPDC_WIDE -DPDC_FORCE_UTF8 -I/mingw64/include/pdcurses -I/ucrt64/include/pdcurses
    LIBS = -lpdcurses_wincon -lcurl -lcjson -lws2_32 -lwinmm
else
    TARGET = novel-cli
    LIBS = -lncurses -lcurl -lcjson
endif

# Installation directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Source directory
SRCDIR = src

# Source files
SRC = $(SRCDIR)/main.c $(SRCDIR)/ui.c $(SRCDIR)/chapter_controller.c $(SRCDIR)/controller.c $(SRCDIR)/network.c $(SRCDIR)/cache.c $(SRCDIR)/library.c $(SRCDIR)/webnovel.c $(SRCDIR)/history.c

# Object directory
OBJDIR = build
# Object files
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
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

# Install dependencies (Windows with MSYS2 MinGW64)
deps-msys2:
	pacman -S --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses mingw-w64-x86_64-curl mingw-w64-x86_64-cjson

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
	@echo "  make deps-msys2   - Install deps (Windows - MSYS2)"
	@echo ""
	@echo "Quick install (recommended):"
	@echo "  chmod +x install.sh && ./install.sh"

.PHONY: all clean re install uninstall deps-arch deps-pacman deps-debian deps-fedora deps-macos deps-msys2 help
