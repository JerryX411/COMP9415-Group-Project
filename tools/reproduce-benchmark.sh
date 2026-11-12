#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
executable="${1:-./build/CoreExplorer}"
destination="${2:-output/benchmarks}"
mkdir -p "$destination"
for level in 0 1 2; do
  "$executable" --benchmark --level "$level" --benchmark-output "$destination/level-$level.csv"
done
