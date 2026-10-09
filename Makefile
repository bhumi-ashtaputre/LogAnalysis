CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic
OMPFLAGS := -fopenmp
BIN := bin

all: $(BIN)/sequential $(BIN)/openmp

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sequential: src/common.cpp src/common.hpp src/sequential.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) src/common.cpp src/sequential.cpp -o $@

$(BIN)/openmp: src/common.cpp src/common.hpp src/openmp.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) src/common.cpp src/openmp.cpp -o $@

clean:
	rm -rf $(BIN) results/*.json results/benchmark.csv

.PHONY: all clean
