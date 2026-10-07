#!/usr/bin/env python3
import csv
from pathlib import Path
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "results" / "benchmark.csv"
OUTDIR = ROOT / "results" / "plots"
OUTDIR.mkdir(parents=True, exist_ok=True)

def read_rows():
    rows = []
    with open(INPUT, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append({
                "records": int(r["records"]),
                "version": r["version"],
                "time_ms": float(r["time_ms"]),
                "speedup": float(r["speedup"]),
            })
    return rows

def main():
    rows = read_rows()
    versions = sorted(set(r["version"] for r in rows))
    sizes = sorted(set(r["records"] for r in rows))

    for version in versions:
        vals = [r["time_ms"] for r in rows if r["version"] == version]
        plt.plot(sizes, vals, marker="o", label=version)

    plt.xlabel("Number of log records")
    plt.ylabel("Execution time (ms)")
    plt.title("Execution Time vs Log Volume")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(OUTDIR / "execution_time.png", dpi=150)
    plt.close()

    for version in versions:
        if version == "Sequential":
            continue
        vals = [r["speedup"] for r in rows if r["version"] == version]
        plt.plot(sizes, vals, marker="o", label=version)

    plt.xlabel("Number of log records")
    plt.ylabel("Speedup")
    plt.title("Parallel Speedup vs Log Volume")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(OUTDIR / "speedup.png", dpi=150)
    plt.close()

    print("Graphs written to", OUTDIR)

if __name__ == "__main__":
    main()
