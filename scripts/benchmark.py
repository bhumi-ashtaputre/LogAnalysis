#!/usr/bin/env python3
"""Run Sequential vs OpenMP performance experiments and save dashboard data."""

import argparse
import csv
import json
import os
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run_program(executable: Path, input_file: Path, output_file: Path, threads: int | None = None) -> float:
    env = os.environ.copy()
    if threads is not None:
        env["OMP_NUM_THREADS"] = str(threads)

    command = [str(executable), str(input_file), str(output_file)]
    completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(
            f"Program failed: {' '.join(command)}\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}"
        )

    with output_file.open(encoding="utf-8") as f:
        result = json.load(f)
    return float(result["analysis_ms"])


def comparable(result: dict) -> dict:
    result = dict(result)
    result.pop("elapsed_ms", None)
    result.pop("analysis_ms", None)
    result.pop("threads", None)
    return result


def generate_input(size: int) -> Path:
    input_file = ROOT / "data" / f"log_{size}.csv"
    subprocess.run(
        [
            "python3", str(ROOT / "tools" / "generate_logs.py"),
            "--lines", str(size),
            "--output", str(input_file),
            "--seed", "42",
        ],
        check=True,
        cwd=ROOT,
    )
    return input_file


def run_size_experiments(sizes: list[int], threads: int, repeats: int) -> list[dict]:
    seq_exe = ROOT / "bin" / "sequential"
    omp_exe = ROOT / "bin" / "openmp"
    rows = []

    for size in sizes:
        input_file = generate_input(size)
        seq_times = []
        omp_times = []

        for repeat in range(repeats):
            seq_output = ROOT / "results" / f"seq_{size}_{repeat}.json"
            omp_output = ROOT / "results" / f"omp_{size}_{repeat}.json"

            seq_times.append(run_program(seq_exe, input_file, seq_output))
            omp_times.append(run_program(omp_exe, input_file, omp_output, threads))

            with seq_output.open(encoding="utf-8") as f:
                seq_result = json.load(f)
            with omp_output.open(encoding="utf-8") as f:
                omp_result = json.load(f)

            if comparable(seq_result) != comparable(omp_result):
                raise RuntimeError(f"Correctness mismatch at input size {size}, repeat {repeat}")

        seq_median = statistics.median(seq_times)
        omp_median = statistics.median(omp_times)
        speedup = seq_median / omp_median if omp_median > 0 else 0.0
        efficiency = (speedup / threads) * 100.0

        rows.append({
            "input_size": size,
            "sequential_ms": round(seq_median, 6),
            "parallel_ms": round(omp_median, 6),
            "speedup": round(speedup, 6),
            "efficiency_percent": round(efficiency, 6),
            "threads": threads,
            "correct": True,
        })

        print(
            f"{size:>8} records | sequential={seq_median:>10.3f} ms | "
            f"parallel={omp_median:>10.3f} ms | speedup={speedup:.3f}x | "
            f"efficiency={efficiency:.2f}%"
        )

    return rows


def run_thread_scaling(size: int, thread_counts: list[int], repeats: int) -> list[dict]:
    """Measure OpenMP scaling for one fixed, sufficiently large input."""
    omp_exe = ROOT / "bin" / "openmp"
    input_file = generate_input(size)
    rows = []

    for threads in thread_counts:
        times = []
        reference = None
        for repeat in range(repeats):
            output = ROOT / "results" / f"thread_{size}_{threads}_{repeat}.json"
            times.append(run_program(omp_exe, input_file, output, threads))
            with output.open(encoding="utf-8") as f:
                result = json.load(f)
            if reference is None:
                reference = result
            elif comparable(reference) != comparable(result):
                raise RuntimeError(f"OpenMP result changed at {threads} threads, repeat {repeat}")

        median_ms = statistics.median(times)
        rows.append({
            "input_size": size,
            "threads": threads,
            "parallel_ms": round(median_ms, 6),
        })
        print(f"{size:>8} records | threads={threads:>2} | parallel={median_ms:>10.3f} ms")

    baseline = next((r["parallel_ms"] for r in rows if r["threads"] == 1), None)
    if baseline is None:
        raise RuntimeError("Thread scaling requires a 1-thread measurement.")

    for row in rows:
        row["speedup"] = round(baseline / row["parallel_ms"], 6) if row["parallel_ms"] > 0 else 0.0
        row["efficiency_percent"] = round((row["speedup"] / row["threads"]) * 100.0, 6)

    return rows


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark Sequential and OpenMP log analysis.")
    parser.add_argument("--sizes", nargs="+", type=int,
                        default=[10000, 50000, 100000, 250000, 500000])
    parser.add_argument("--threads", type=int, default=4,
                        help="OpenMP thread count used for input-size scaling.")
    parser.add_argument("--thread-counts", nargs="+", type=int,
                        default=[1, 2, 4],
                        help="Thread counts used for thread-scaling experiment.")
    parser.add_argument("--thread-test-size", type=int, default=500000,
                        help="Fixed input size used for thread-scaling experiment.")
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()

    if args.threads < 1 or args.repeats < 1:
        raise SystemExit("--threads and --repeats must be positive.")
    if any(t < 1 for t in args.thread_counts):
        raise SystemExit("All --thread-counts values must be positive.")
    if 1 not in args.thread_counts:
        raise SystemExit("--thread-counts must include 1 for the scaling baseline.")

    (ROOT / "data").mkdir(exist_ok=True)
    (ROOT / "results").mkdir(exist_ok=True)

    seq_exe = ROOT / "bin" / "sequential"
    omp_exe = ROOT / "bin" / "openmp"
    if not seq_exe.exists() or not omp_exe.exists():
        raise SystemExit("Build the project first with: make")

    print("\nINPUT-SIZE EXPERIMENT")
    size_rows = run_size_experiments(args.sizes, args.threads, args.repeats)
    write_csv(ROOT / "results" / "benchmark.csv", size_rows)

    print("\nTHREAD-SCALING EXPERIMENT")
    thread_rows = run_thread_scaling(args.thread_test_size, args.thread_counts, args.repeats)
    write_csv(ROOT / "results" / "thread_benchmark.csv", thread_rows)

    print("\nSaved:")
    print(f"  {ROOT / 'results' / 'benchmark.csv'}")
    print(f"  {ROOT / 'results' / 'thread_benchmark.csv'}")
    print("Correctness was checked during every experiment.")


if __name__ == "__main__":
    main()
