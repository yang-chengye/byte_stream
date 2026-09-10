#!/usr/bin/env python3

import argparse
import csv
import math
import sys


def read_results(path):
    with open(path, newline="", encoding="utf-8") as source:
        rows = csv.DictReader(source)
        required = {"benchmark", "median_ns"}
        if rows.fieldnames is None or not required.issubset(rows.fieldnames):
            raise ValueError(f"{path}: expected CSV columns {sorted(required)}")

        results = {}
        for line_number, row in enumerate(rows, start=2):
            name = row["benchmark"]
            if not name:
                raise ValueError(f"{path}:{line_number}: empty benchmark name")
            if name in results:
                raise ValueError(f"{path}:{line_number}: duplicate benchmark {name!r}")
            value = float(row["median_ns"])
            if not math.isfinite(value) or value <= 0.0:
                raise ValueError(f"{path}:{line_number}: invalid median_ns for {name!r}")
            results[name] = value
        return results


def main():
    parser = argparse.ArgumentParser(description="Compare byte_stream benchmark CSV results")
    parser.add_argument("baseline")
    parser.add_argument("current")
    parser.add_argument("--max-regression-percent", type=float, default=5.0)
    args = parser.parse_args()

    if not math.isfinite(args.max_regression_percent) or args.max_regression_percent < 0.0:
        parser.error("--max-regression-percent must be a finite non-negative number")

    try:
        baseline = read_results(args.baseline)
        current = read_results(args.current)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    baseline_names = set(baseline)
    current_names = set(current)
    missing = sorted(baseline_names - current_names)
    added = sorted(current_names - baseline_names)
    if missing or added:
        if missing:
            print("missing benchmarks: " + ", ".join(missing), file=sys.stderr)
        if added:
            print("new benchmarks: " + ", ".join(added), file=sys.stderr)
        return 2

    regressions = []
    for name in sorted(baseline):
        change = (current[name] / baseline[name] - 1.0) * 100.0
        print(f"{name}: {change:+.2f}%")
        if change > args.max_regression_percent:
            regressions.append((name, change))

    if regressions:
        print(
            f"performance regression: {len(regressions)} benchmark(s) exceeded "
            f"{args.max_regression_percent:.2f}%",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
