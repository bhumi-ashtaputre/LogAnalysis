# Server Log Analytics: Sequential vs OpenMP

## 1. Project overview

This project processes application/server logs and identifies useful patterns, events, and anomalies. The same analysis is implemented in two ways:

1. **Sequential C++** — single-threaded baseline.
2. **OpenMP C++** — parallel CPU implementation using multiple threads.

The project verifies that both implementations produce the same analytical result and evaluates their performance for increasing log volumes and different OpenMP thread counts.

The browser dashboard is implemented entirely in `dashboard/app.py`. It reads the experimental CSV/JSON results and creates all required graphs and tables directly from that file.

## 2. Input format

Each CSV record is:

```text
Timestamp,IP,URL,HTTP_Status,Response_Time_ms
```

Example:

```text
2026-01-01 00:00:01,192.168.1.14,/login,200,143.20
```

## 3. Log analysis performed

For every log record the program calculates:

- total requests
- successful requests
- failed requests
- HTTP status counts
- top requested URLs
- top client IPs
- average response time
- average response time per URL

It also checks for explainable patterns such as:

- consecutive 5xx error bursts
- timestamp-based traffic spikes
- repeated failed login attempts from one IP
- unusually high-volume IPs
- slow endpoints

The anomaly rules are explicit and are implemented in `src/common.cpp`.

## 4. Parallelization strategy

The OpenMP implementation divides the input records into contiguous ranges. Each thread creates a private `AnalysisResult`, processes its assigned records, and the partial results are merged after the parallel region.

```text
Input records
       |
       +-------------------------------+
       |               |               |
    Thread 0        Thread 1        Thread N-1
       |               |               |
   local result    local result    local result
       +---------------+---------------+
                       |
                 merge results
                       |
                anomaly analysis
                       |
                   JSON output
```

This avoids a shared lock in the main record-processing loop.

## 5. Correctness verification

`scripts/verify.py` runs both implementations on the same input and compares the analytical output after removing runtime-only fields (`elapsed_ms`, `analysis_ms`, and `threads`).

`scripts/benchmark.py` performs the same correctness check for every tested input size and every repeated measurement.

## 6. Performance evaluation

The project evaluates:

- execution time of Sequential and OpenMP for increasing input sizes
- speedup of OpenMP relative to Sequential
- parallel efficiency for the selected OpenMP thread count
- effect of increasing the number of OpenMP threads for a fixed large input
- correctness of the parallel result

The dashboard presents the experimental results using:

1. Execution Time vs Input Size
2. Speedup vs Input Size
3. Parallel Efficiency vs Input Size
4. OpenMP Execution Time vs Number of Threads
5. Sequential vs OpenMP performance table
6. OpenMP thread-scaling table
7. Sequential vs OpenMP correctness table


## 7. Build

From the project root:

```bash
make
```

This creates only:

```text
bin/sequential
bin/openmp
```

## 8. Generate test data

```bash
python3 tools/generate_logs.py --lines 10000 --output data/log_10k.csv --seed 42
```

The generator creates deterministic records and controlled patterns for testing the analysis functions.

## 9. Run Sequential

```bash
./bin/sequential data/log_10k.csv results/sequential.json
```

## 10. Run OpenMP

For four threads:

```bash
OMP_NUM_THREADS=4 ./bin/openmp data/log_10k.csv results/openmp.json
```

On Windows PowerShell:

```powershell
$env:OMP_NUM_THREADS=4
.\bin\openmp.exe data\log_10k.csv results\openmp.json
```

## 11. Verify correctness

```bash
python3 scripts/verify.py data/log_10k.csv
```

Expected output:

```text
VERIFICATION PASSED: sequential and OpenMP results are identical.
```

## 12. Run the complete performance experiment

```bash
python3 scripts/benchmark.py --sizes 10000 50000 100000 250000 500000 --threads 4 --thread-counts 1 2 4 --thread-test-size 500000 --repeats 3
```

The benchmark:

1. generates each required input size,
2. runs Sequential,
3. runs OpenMP,
4. repeats measurements,
5. uses the median core-analysis time,
6. verifies correctness,
7. calculates the performance metrics,
8. measures OpenMP thread scaling,
9. saves the data used by the dashboard.

Generated result files:

```text
results/benchmark.csv
results/thread_benchmark.csv
```

## 13. Start the dashboard

```bash
python3 dashboard/app.py
```

Open:

```text
http://127.0.0.1:8000
```

The dashboard is self-contained. No Flask installation or separate graphing program is required.

## 14. Project files

```text
log_analysis_project/
├── src/
│   ├── common.hpp
│   ├── common.cpp
│   ├── sequential.cpp
│   └── openmp.cpp
├── tools/
│   └── generate_logs.py
├── scripts/
│   ├── benchmark.py
│   ├── smoke_test.py
│   └── verify.py
├── dashboard/
│   └── app.py
├── data/
├── results/
├── Makefile
├── README.md
└── ARCHITECTURE.md
```

