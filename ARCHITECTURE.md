# Architecture

## Processing pipeline

```text
CSV server logs
      |
      v
  CSV parser
      |
      v
vector<LogRecord>
      |
      +---------------------------+
      |                           |
      v                           v
Sequential analysis        OpenMP parallel analysis
      |                           |
      |                     local AnalysisResult
      |                           |
      |                      merge local results
      +-------------+-------------+
                    |
                    v
             anomaly detection
                    |
                    v
                JSON result
                    |
                    v
             dashboard/app.py
                    |
                    v
       graphs + tables + interpretation
```

## OpenMP design

The computationally intensive record aggregation is split into contiguous ranges. Each OpenMP thread owns a separate `AnalysisResult` object.

This avoids a shared update such as:

```text
shared_counter++
```

inside the main loop, which would require synchronization and could create a race condition.

After the parallel region, the program merges the per-thread maps and counters.

## Dashboard design

`dashboard/app.py` is intentionally self-contained. It:

- reads `results/sequential.json`
- reads `results/openmp.json`
- reads `results/benchmark.csv`
- calculates/reads speedup and efficiency values
- renders SVG graphs directly in HTML
- renders experimental tables
- explains how to interpret speedup and efficiency

No separate graph-generation server is needed.
