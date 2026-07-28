#!/usr/bin/env bash
# M6: measures the compile-time win from the runtime PCH (runtime_pch/).
#
# ebc has no flag to disable PCH lookup (deliberate - see the M6 plan: no
# "is this GCC" detection, no opt-out, since an absent/mismatched .gch is
# already a safe, silent no-op). So this script isolates exactly what the
# PCH speeds up - the backend `g++ -c` step - by invoking g++ directly on
# ebc's own generated .cpp, once with the PCH shadow directory on the -I
# path and once without, rather than trying to toggle ebc itself. It also
# reports the full end-to-end `ebc` time for context (the PCH-affected step
# is only part of the whole pipeline: preprocessing/lexing/parsing/codegen
# happen first, unaffected by this optimization).
#
# Deliberately a hand-run script, not a ctest case: asserting on absolute
# wall-clock thresholds in CI is inherently flaky across machines/load, so
# this reports numbers for a human (or roadmap notes) to read, rather than
# gating a pass/fail test on them.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: bench_pch.sh <build-dir>  (e.g. build/linux-gcc)" >&2
    exit 2
fi

BUILD_DIR="$(cd "$1" && pwd)"
EBC="$BUILD_DIR/compiler/ebc"
PCH_DIR="$BUILD_DIR/runtime_pch"
RUNTIME_INCLUDE_DIR="$(cd "$BUILD_DIR/../.." && pwd)/runtime/include"
RUNS=10

if [ ! -x "$EBC" ]; then
    echo "error: $EBC not found or not executable - build it first" >&2
    exit 1
fi
if [ ! -f "$PCH_DIR/ebasic/runtime/runtime.hpp.gch" ]; then
    echo "error: $PCH_DIR/ebasic/runtime/runtime.hpp.gch not found - g++ may be missing, or the build is stale" >&2
    exit 1
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

cat > "$WORKDIR/bench.bas" <<'EOF'
DIM total AS INTEGER
DIM i AS INTEGER
FOR i = 1 TO 10
    total = total + i
NEXT i
PRINT total
EOF

avg_time() {
    local total="0" i
    for ((i = 0; i < RUNS; i++)); do
        local start end
        start=$(date +%s.%N)
        "$@"
        end=$(date +%s.%N)
        total=$(echo "$total + ($end - $start)" | bc)
    done
    echo "scale=4; $total / $RUNS" | bc
}

echo "=== Full 'ebc <file.bas> -o <out>' invocation (PCH always used automatically) ==="
FULL_EBC=$(avg_time "$EBC" "$WORKDIR/bench.bas" -o "$WORKDIR/bench_out" --keep-cpp)
echo "avg ${FULL_EBC}s over $RUNS runs"
echo

GEN_CPP="$WORKDIR/bench_out.gen.cpp"

echo "=== Isolated backend 'g++ -c' step on the generated .cpp - what the PCH actually speeds up ==="
WITH_PCH=$(avg_time g++ -std=c++17 -I "$PCH_DIR" -I "$RUNTIME_INCLUDE_DIR" -c "$GEN_CPP" -o "$WORKDIR/bench_out.o")
WITHOUT_PCH=$(avg_time g++ -std=c++17 -I "$RUNTIME_INCLUDE_DIR" -c "$GEN_CPP" -o "$WORKDIR/bench_out.o")
echo "with PCH:    avg ${WITH_PCH}s over $RUNS runs"
echo "without PCH: avg ${WITHOUT_PCH}s over $RUNS runs"
REDUCTION=$(echo "scale=1; (1 - $WITH_PCH / $WITHOUT_PCH) * 100" | bc)
echo "reduction: ${REDUCTION}%"
