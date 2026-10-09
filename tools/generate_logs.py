#!/usr/bin/env python3
import argparse
import random
from datetime import datetime, timedelta

URLS = ["/", "/home", "/login", "/products", "/search", "/cart", "/checkout", "/payment", "/profile"]
NORMAL_IPS = [f"192.168.1.{i}" for i in range(10, 40)]
ATTACK_IPS = ["10.0.0.99", "10.0.0.100"]


def generate(lines: int, output: str, seed: int) -> None:
    rng = random.Random(seed)
    start = datetime(2026, 1, 1, 0, 0, 0)

    with open(output, "w", encoding="utf-8") as f:
        for i in range(lines):
            # Controlled patterns make the anomaly detector testable.
            if 1000 <= i < min(lines, 1100):
                ip, url, status = ATTACK_IPS[0], "/login", 401
                timestamp = start + timedelta(seconds=120)
            elif 5000 <= i < min(lines, 5200):
                ip = rng.choice(NORMAL_IPS)
                url, status = rng.choice(["/checkout", "/payment"]), 500
                timestamp = start + timedelta(seconds=300)
            elif 15000 <= i < min(lines, 15100):
                ip, url, status = ATTACK_IPS[1], "/login", 401
                timestamp = start + timedelta(seconds=420)
            elif 20000 <= i < min(lines, 20100):
                ip, url, status = ATTACK_IPS[1], "/login", 200
                timestamp = start + timedelta(seconds=600)
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
                timestamp = start + timedelta(seconds=i)

            if url in ("/checkout", "/payment") and rng.random() < 0.20:
                response = rng.uniform(1000, 2500)
            else:
                response = rng.uniform(20, 500)

            f.write(
                f"{timestamp:%Y-%m-%d %H:%M:%S},{ip},{url},{status},{response:.2f}\n"
            )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate deterministic server-log CSV data.")
    parser.add_argument("--lines", type=int, required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    generate(args.lines, args.output, args.seed)
    print(f"Generated {args.lines} records -> {args.output}")
