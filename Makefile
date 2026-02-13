# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -g

# Libraries
LIBS = -lncurses -lcurl -lcjson -lmyhtml

# Source files
SRC = main.c ui.c chapter_controller.c controller.c network.c cache.c library.c webnovel.c

# Object files
OBJ = $(SRC:.c=.o)

# Output binary
TARGET = novel

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LIBS)

# Compile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJ) $(TARGET)

# Rebuild everything
re: clean all
