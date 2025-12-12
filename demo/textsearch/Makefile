# Makefile for textsearch demo
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -std=c99
LDFLAGS = 

# Source files
TEXTSEARCH_SOURCES = textsearch.c ts_kmp.c ts_bm.c
TEST_SOURCES = test_textsearch.c

# Object files
TEXTSEARCH_OBJECTS = $(TEXTSEARCH_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Target executable
TARGET = test_textsearch

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(TEXTSEARCH_OBJECTS) $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "Running textsearch demo..."
	./$(TARGET)

clean:
	rm -f $(TEXTSEARCH_OBJECTS) $(TEST_OBJECTS) $(TARGET)

# Install dependencies (placeholder)
install-deps:
	@echo "No external dependencies required for this demo"

help:
	@echo "Available targets:"
	@echo "  all         - Build the test program"
	@echo "  test        - Build and run the test program"
	@echo "  clean       - Remove build artifacts"
	@echo "  install-deps- Install dependencies (placeholder)"
	@echo "  help        - Show this help message"