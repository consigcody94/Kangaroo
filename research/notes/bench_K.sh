#!/usr/bin/env bash
# Collect K samples by running N independent single-solve benchmarks.
# Each run uses a fresh process → fresh random key, no DP-table carryover bug.
cd /c/Users/Ajs/Desktop/Kangaroo
N="${1:-10}"
RANGE="${2:-66}"
DP="${3:-14}"

echo "=== K-sampling: N=$N, range=$RANGE, dp=$DP ==="
TOTAL=0
COUNT=0
SAMPLES=""

for i in $(seq 1 "$N"); do
    t0=$(date +%s)
    OUT=$(timeout 60 ./RCKangaroo.exe -dp "$DP" -range "$RANGE" -max 3 2>&1)
    t1=$(date +%s)
    # Grab the first "Point solved, K:" line
    K=$(echo "$OUT" | grep -m1 "Point solved, K:" | awk '{print $4}')
    ERR=$(echo "$OUT" | grep -m1 "Collision Error" || true)
    if [ -z "$K" ]; then
        echo "run $i: NO SOLVE in $((t1-t0))s"
    else
        echo "run $i: K=$K   elapsed=$((t1-t0))s   err=$ERR"
        SAMPLES="$SAMPLES $K"
        TOTAL=$(echo "$TOTAL + $K" | bc -l)
        COUNT=$((COUNT+1))
    fi
done

if [ "$COUNT" -gt 0 ]; then
    MEAN=$(echo "$TOTAL / $COUNT" | bc -l)
    echo "=== Mean K over $COUNT solves: $MEAN ==="
    echo "Samples: $SAMPLES"
fi
