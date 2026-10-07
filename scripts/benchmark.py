#!/usr/bin/env python3
import csv
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "tools" / "generate_logs.py"
BIN = ROOT / "bin"
DATA = ROOT / "data"
RESULTS = ROOT / "results"
RESULTS.mkdir(exist_ok=True)
DATA.mkdir(exist_ok=True)

# Reduce this list if your computer has limited RAM/storage.
SIZES = [10_000, 100_000, 500_000, 1_000_000]
OMP_THREADS = "4"
MPI_PROCESSES = "2"

def run_timed(cmd, env=None):
    start = time.perf_counter()
    p = subprocess.run(cmd, cwd=ROOT, env=env, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    elapsed = (time.perf_counter() - start) * 1000
    if p.returncode != 0:
        print(p.stdout)
        print(p.stderr)
        raise RuntimeError("Failed: " + " ".join(map(str, cmd)))
    return elapsed

def main():
    rows = []

    for n in SIZES:
        dataset = DATA / f"log_{n}.csv"
        if not dataset.exists():
            subprocess.run([
                "python3", str(GEN), "--lines", str(n),
                "--output", str(dataset), "--seed", "42"
            ], check=True)

        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = OMP_THREADS

        seq_ms = run_timed([
            str(BIN / "sequential"), str(dataset), str(RESULTS / "bench_seq.json")
        ], env)

        omp_ms = run_timed([
            str(BIN / "openmp"), str(dataset), str(RESULTS / "bench_omp.json")
        ], env)

        hybrid_ms = run_timed([
            "mpirun", "--oversubscribe", "-np", MPI_PROCESSES,
            str(BIN / "hybrid"), str(dataset), str(RESULTS / "bench_hybrid.json")
        ], env)

        naive_ms = run_timed([
            str(BIN / "openmp_naive"), str(dataset), str(RESULTS / "bench_naive.json")
        ], env)

        rows.extend([
            [n, "Sequential", seq_ms, 1.0],
            [n, "OpenMP", omp_ms, seq_ms / omp_ms if omp_ms else 0],
            [n, "Naive OpenMP", naive_ms, seq_ms / naive_ms if naive_ms else 0],
            [n, "MPI+OpenMP", hybrid_ms, seq_ms / hybrid_ms if hybrid_ms else 0],
        ])

    out = RESULTS / "benchmark.csv"
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["records", "version", "time_ms", "speedup"])
        w.writerows(rows)

    print("Benchmark complete:", out)

if __name__ == "__main__":
    main()
