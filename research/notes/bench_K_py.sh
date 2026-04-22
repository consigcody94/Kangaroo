#!/usr/bin/env bash
# N-sample K measurement using Python for stats (bc not available).
cd /c/Users/Ajs/Desktop/Kangaroo
N="${1:-20}"
RANGE="${2:-66}"
DP="${3:-14}"

echo "=== K-sampling: N=$N, range=$RANGE, dp=$DP ==="
SAMPLES_FILE=/tmp/rck_samples.txt
> "$SAMPLES_FILE"

for i in $(seq 1 "$N"); do
    t0=$(date +%s)
    OUT=$(timeout 90 ./RCKangaroo.exe -dp "$DP" -range "$RANGE" -max 3 2>&1)
    t1=$(date +%s)
    K=$(echo "$OUT" | grep -m1 "Point solved, K:" | awk '{print $4}')
    ERR=$(echo "$OUT" | grep -c "Collision Error")
    if [ -z "$K" ]; then
        echo "run $i: NO_SOLVE elapsed=$((t1-t0))s"
    else
        echo "run $i: K=$K elapsed=$((t1-t0))s err=$ERR"
        echo "$K" >> "$SAMPLES_FILE"
    fi
done

python - <<'EOF'
import statistics as st
with open('/tmp/rck_samples.txt') as f:
    xs = [float(x) for x in f.read().split() if x]
print("---")
print(f"N solved: {len(xs)}")
if len(xs) >= 2:
    print(f"mean   = {st.mean(xs):.3f}")
    print(f"median = {st.median(xs):.3f}")
    print(f"stdev  = {st.stdev(xs):.3f}")
    print(f"min    = {min(xs):.3f}")
    print(f"max    = {max(xs):.3f}")
    se = st.stdev(xs)/(len(xs)**0.5)
    print(f"95% CI of mean: {st.mean(xs):.3f} +/- {1.96*se:.3f}")
EOF
