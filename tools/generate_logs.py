#!/usr/bin/env python3
import argparse
import random
from datetime import datetime, timedelta

URLS = ["/", "/home", "/login", "/products", "/search", "/cart", "/checkout", "/payment", "/profile"]
NORMAL_IPS = [f"192.168.1.{i}" for i in range(10, 40)]
ATTACK_IPS = ["10.0.0.99", "10.0.0.100"]

def generate(args):
    rng = random.Random(args.seed)
    start = datetime(2026, 1, 1, 0, 0, 0)

    with open(args.output, "w", encoding="utf-8") as f:
        for i in range(args.lines):
            # Concentrated suspicious traffic at deterministic intervals.
            if 1000 <= i < min(args.lines, 1100):
                ip = ATTACK_IPS[0]
                status = 401
                url = "/login"
            elif 5000 <= i < min(args.lines, 5200):
                ip = NORMAL_IPS[rng.randrange(len(NORMAL_IPS))]
                status = 500
                url = rng.choice(["/checkout", "/payment"])
            elif 15000 <= i < min(args.lines, 15100):
                ip = ATTACK_IPS[1]
                status = 401
                url = "/login"
            else:
                ip = rng.choice(NORMAL_IPS)
                url = rng.choice(URLS)

                roll = rng.random()
                if roll < 0.92:
                    status = 200
                elif roll < 0.96:
                    status = 404
                elif roll < 0.99:
                    status = 401
                else:
                    status = 500

            if url in ("/checkout", "/payment") and rng.random() < 0.20:
                response = rng.uniform(1000, 2500)
            else:
                response = rng.uniform(20, 500)

            # Controlled anomaly windows share the same minute so the
            # deterministic time-window detectors can identify them.
            if 1000 <= i < min(args.lines, 1100):
                timestamp = start + timedelta(seconds=120)
            elif 2000 <= i < min(args.lines, 2100):
                timestamp = start + timedelta(seconds=180)
            elif 5000 <= i < min(args.lines, 5200):
                timestamp = start + timedelta(seconds=300)
            elif 15000 <= i < min(args.lines, 15100):
                timestamp = start + timedelta(seconds=420)
            else:
                timestamp = start + timedelta(seconds=i)
            f.write(
                f"{timestamp:%Y-%m-%d %H:%M:%S},"
                f"{ip},{url},{status},{response:.2f}\n"
            )

if __name__ == "__main__":
    p = argparse.ArgumentParser(description="Generate deterministic server logs.")
    p.add_argument("--lines", type=int, required=True)
    p.add_argument("--output", required=True)
    p.add_argument("--seed", type=int, default=42)
    args = p.parse_args()
    generate(args)
    print(f"Generated {args.lines} records -> {args.output}")
