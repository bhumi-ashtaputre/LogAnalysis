# Architecture

## Core pipeline

```text
CSV Log
  |
  v
Parsing
  |
  +---------------------------+
  |                           |
  v                           v
Statistics                Anomaly rules
  |                           |
  +-------------+-------------+
                |
                v
             JSON
                |
                v
           Dashboard
```

## Parallel designs

### Sequential

One execution flow processes every valid record.

### OpenMP

The record array is divided into contiguous ranges. Each OpenMP thread writes into its own `AnalysisResult`. Results are merged after the parallel region.

This avoids a shared lock for every record.

### Naive OpenMP

All threads update one shared result inside a critical section. This is intentionally inefficient and exists to demonstrate lock contention.

### MPI + OpenMP

MPI divides the record range between processes. Each process then divides its own range between OpenMP threads. Each process produces a partial result and sends it to rank 0. Rank 0 merges the partial results and writes the final JSON.

## Important implementation note

For clarity and portability, the hybrid implementation currently has every MPI process read the CSV file independently, then analyze a disjoint range. This demonstrates process-level and thread-level parallelism without introducing MPI-IO complexity.

For a production-scale implementation, the next optimization would be distributed file I/O / MPI-IO or a byte-range partitioning strategy so that each process reads only its assigned part of the file.
