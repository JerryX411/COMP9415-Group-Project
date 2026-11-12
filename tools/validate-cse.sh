#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="${1:-build-cse}"
cmake -S . -B "$build_dir" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_LITE_EDITOR=OFF
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
for level in 0 1 2; do
  "$build_dir/CoreExplorerLite" --play --level "$level" --frames 3 --capture "$build_dir/cse-level-$level.png"
done
"$build_dir/CoreExplorerLite" --effect gate --frames 100 --capture "$build_dir/cse-gate-open.png"
printf '%s\n' 'CSE build, tests and OpenGL smoke checks passed. Interactive controls still need a desktop check.'
