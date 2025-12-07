# Makefile for textsearch demo
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -std=c99
LDFLAGS = 

# Source files
TEXTSEARCH_SOURCES = textsearch.c ts_kmp.c ts_bm.c ts_wildcard.c ts_router.c
TEST_SOURCES = test_textsearch.c

# Object files
TEXTSEARCH_OBJECTS = $(TEXTSEARCH_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Target executable
TARGET = test_textsearch
SIMPLE_TARGET = test_wildcard_simple
BENCHMARK_TARGET = stress_test
COMP_TARGET = perf_compare
URL_TARGET = url_benchmark
ROUTER_TARGET = router_comparison

.PHONY: all clean test simple benchmark compare url router full-test

all: $(TARGET)

$(TARGET): $(TEXTSEARCH_OBJECTS) $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(SIMPLE_TARGET): $(TEXTSEARCH_OBJECTS) test_wildcard_simple.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BENCHMARK_TARGET): $(TEXTSEARCH_OBJECTS) benchmark.o
	$(CC) $(LDFLAGS) -o $@ $^

$(COMP_TARGET): $(TEXTSEARCH_OBJECTS) simple_benchmark.o
	$(CC) $(LDFLAGS) -o $@ $^

$(URL_TARGET): $(TEXTSEARCH_OBJECTS) url_benchmark.o
	$(CC) $(LDFLAGS) -o $@ $^

$(ROUTER_TARGET): $(TEXTSEARCH_OBJECTS) router_comparison.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "Running textsearch demo..."
	./$(TARGET)

simple: $(SIMPLE_TARGET)
	@echo "Running simple wildcard test..."
	./$(SIMPLE_TARGET)

benchmark: $(BENCHMARK_TARGET)
	@echo "Running stress test and benchmark..."
	./$(BENCHMARK_TARGET)

compare: $(COMP_TARGET)
	@echo "Running simple performance comparison..."
	./$(COMP_TARGET)

url: $(URL_TARGET)
	@echo "Running URL matching benchmark..."
	./$(URL_TARGET)

router: $(ROUTER_TARGET)
	@echo "Running router algorithm comparison..."
	./$(ROUTER_TARGET)

clean:
	rm -f $(TEXTSEARCH_OBJECTS) $(TEST_OBJECTS) benchmark.o simple_benchmark.o url_benchmark.o router_comparison.o $(TARGET) $(SIMPLE_TARGET) $(BENCHMARK_TARGET) $(COMP_TARGET) $(URL_TARGET) $(ROUTER_TARGET)

# Install dependencies (placeholder)
install-deps:
	@echo "No external dependencies required for this demo"

full-test: $(TARGET) $(SIMPLE_TARGET) $(BENCHMARK_TARGET) $(COMP_TARGET) $(URL_TARGET) $(ROUTER_TARGET)
	@echo "========================================"
	@echo "       Complete Textsearch Test Suite"
	@echo "========================================"
	@echo ""
	@echo "1. Basic algorithm functionality test:"
	@./$(TARGET) | head -30
	@echo ""
	@echo "2. Wildcard pattern matching test:"
	@./$(SIMPLE_TARGET)
	@echo ""
	@echo "3. Stress test and performance benchmark:"
	@./$(BENCHMARK_TARGET) | tail -20
	@echo ""
	@echo "4. Algorithm performance comparison:"
	@./$(COMP_TARGET) | tail -10
	@echo ""
	@echo "5. URL matching benchmark:"
	@./$(URL_TARGET) | tail -15
	@echo ""
	@echo "6. Router algorithm comparison:"
	@./$(ROUTER_TARGET) | tail -15
	@echo ""
	@echo "========================================"
	@echo "          All tests completed!"
	@echo "========================================"

help:
	@echo "Available targets:"
	@echo "  all         - Build the test program"
	@echo "  test        - Build and run the test program"
	@echo "  simple      - Build and run simple wildcard test"
	@echo "  benchmark   - Build and run full stress test"
	@echo "  compare     - Build and run performance comparison"
	@echo "  url         - Build and run URL matching benchmark"
	@echo "  router      - Build and run router algorithm comparison"
	@echo "  full-test   - Run complete test suite"
	@echo "  clean       - Remove build artifacts"
	@echo "  install-deps- Install dependencies (placeholder)"
	@echo "  help        - Show this help message"