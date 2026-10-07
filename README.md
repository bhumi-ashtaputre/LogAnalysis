# Parallel Server Log Analysis

A parallel programming project for processing large server/application logs and identifying patterns and anomalies.

## What this project demonstrates

Three implementations are provided:

1. **Sequential** C++ baseline
2. **OpenMP** shared-memory parallel implementation
3. **MPI + OpenMP** hybrid implementation

The implementations perform the same deterministic analysis and should produce the same logical report.

### Analysis performed

- Total requests
- Successful / failed requests
- HTTP status-code counts
- Average response time
- Slowest URL by average response time
- Most-requested URL
- Most-active IP
- Error-burst anomalies (5+ consecutive 5xx responses)
- Traffic-spike detection using fixed record/time-density windows
- Brute-force login candidates (5+ HTTP 401 responses to /login from one IP)
- High-volume IP detection
- Slow URL detection
- Top suspicious IPs

### Extra features

- Automated correctness verification
- Empty/small/edge-case datasets
- Naive OpenMP implementation to demonstrate synchronization overhead
- JSON output
- Browser dashboard
- Benchmarking for increasing dataset sizes
- Speedup calculation
- Plot generation

## Project structure

```text
log_analysis_project/
├── src/
│   ├── common.hpp
│   ├── common.cpp
│   ├── sequential.cpp
│   ├── openmp.cpp
│   ├── openmp_naive.cpp
│   └── hybrid_mpi_openmp.cpp
├── tools/
│   └── generate_logs.py
├── scripts/
│   ├── verify.py
│   ├── benchmark.py
│   └── plot_results.py
├── dashboard/
│   ├── app.py
│   └── templates/
│       └── index.html
├── data/
├── results/
├── Makefile
└── README.md
```

## Requirements

Recommended environment:

- Ubuntu / WSL2
- g++
- OpenMP support
- Open MPI (`mpic++`, `mpirun`)
- Python 3
- Flask
- matplotlib

Install on Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential openmpi-bin libopenmpi-dev python3 python3-pip
python3 -m pip install flask matplotlib
```

## Build

```bash
make
```

This creates:

```text
bin/sequential
bin/openmp
bin/openmp_naive
bin/hybrid
```

## Generate test logs

Small test file:

```bash
python3 tools/generate_logs.py --lines 10000 --output data/log_10k.csv --seed 42
```

Larger file:

```bash
python3 tools/generate_logs.py --lines 1000000 --output data/log_1m.csv --seed 42
```

The generator deliberately inserts deterministic anomaly patterns so they can be tested.

## Run sequential

```bash
./bin/sequential data/log_10k.csv results/sequential.json
```

## Run OpenMP

```bash
OMP_NUM_THREADS=4 ./bin/openmp data/log_10k.csv results/openmp.json
```

## Run naive OpenMP

```bash
OMP_NUM_THREADS=4 ./bin/openmp_naive data/log_10k.csv results/openmp_naive.json
```

The naive version intentionally uses a shared critical section for aggregation. It is included for educational comparison and is expected to be slower on sufficiently large datasets.

## Run hybrid MPI + OpenMP

Example with 2 MPI processes and 2 OpenMP threads per process:

```bash
OMP_NUM_THREADS=2 mpirun --oversubscribe -np 2 ./bin/hybrid data/log_10k.csv results/hybrid.json
```

The exact MPI command may differ by MPI installation.

## Verify correctness

```bash
python3 scripts/verify.py data/log_10k.csv
```

The verification script runs all three main implementations and compares their normalized JSON reports.

## Benchmark

```bash
python3 scripts/benchmark.py
```

The benchmark uses several dataset sizes. Edit `scripts/benchmark.py` if your machine cannot handle the largest size.

Results are written to:

```text
results/benchmark.csv
```

## Generate graphs

```bash
python3 scripts/plot_results.py
```

Graphs are saved under:

```text
results/plots/
```

## Start dashboard

```bash
python3 dashboard/app.py
```

Then open:

```text
http://127.0.0.1:5000
```

The dashboard reads the JSON result files from `results/`.

## Important correctness idea

The parallel programs should not be judged only by speed.

The project requires:

```text
Sequential result
       =
OpenMP result
       =
MPI + OpenMP result
```

for the same input.

The verification script also checks edge cases such as:

- empty file
- one-record file
- normal data
- repeated failed logins
- error-heavy data

## Speedup

For a parallel implementation:

```text
Speedup = Sequential time / Parallel time
```

Example:

```text
Sequential = 10 seconds
OpenMP = 4 seconds

Speedup = 10 / 4 = 2.5x
```

Do not use example numbers in the report. Use measurements from your machine.

## Dataset

This project includes a deterministic synthetic-log generator. For the final submission, you can either:

1. submit/link the generated dataset, or
2. use a public server-log dataset and record its original source link.

The generated dataset is useful for controlled experiments because the size and anomaly patterns can be reproduced using the same seed.

## AI/prompt-log note

Do not put your AI prompt log in this repository if your instructor says prompts must not be on GitHub.

Keep the prompt log separately for submission.
