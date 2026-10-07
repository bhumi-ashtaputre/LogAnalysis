CXX := g++
MPICXX := mpic++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra
OMPFLAGS := -fopenmp
BIN := bin

all: $(BIN)/sequential $(BIN)/openmp $(BIN)/openmp_naive $(BIN)/hybrid

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sequential: src/common.cpp src/sequential.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) src/common.cpp src/sequential.cpp -o $@

$(BIN)/openmp: src/common.cpp src/openmp.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) src/common.cpp src/openmp.cpp -o $@

$(BIN)/openmp_naive: src/common.cpp src/openmp_naive.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) src/common.cpp src/openmp_naive.cpp -o $@

$(BIN)/hybrid: src/common.cpp src/hybrid_mpi_openmp.cpp | $(BIN)
	$(MPICXX) $(CXXFLAGS) $(OMPFLAGS) src/common.cpp src/hybrid_mpi_openmp.cpp -o $@

clean:
	rm -rf $(BIN) results/*.json results/benchmark.csv results/plots

.PHONY: all clean
