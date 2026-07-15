#!/usr/bin/env bash
# Baseline / benchmark harness for the command center. The REAL one, as shipped — same reason
# shot.sh is a headless browser and not a second renderer: render_native.c drifted from the browser
# and benchmarked a scene nobody ran.
#
#   ./bench.sh [REPEATS] [MEASURE_S] [OUT_JSON]
#
# Produces per-run JSON, a screenshot per run (scored by pngstat.py), and a baseline file with the
# NOISE FLOOR — the run-to-run spread that a later change has to beat before it may be called an
# improvement or a regression. That threshold, not the absolute numbers, is the point of this.
#
# WHAT THIS DOES NOT MEASURE — read before quoting a number from it:
#   * NOT GPU cost. Headless chromium here runs SwiftShader, i.e. the "GPU" is the CPU, and this
#     box has no GPU at all. Measured: gl.finish() costs 0.00 ms p50 (7 ms total over 12 s), so the
#     scene is not raster-bound here and these numbers say nothing about a real GPU.
#   * NOT frame rate, and NOT frame time. Headless chromium paces an idle page at ~131 ms while the
#     frame itself costs 0.5 ms — ~130 of every 131 ms is the browser waiting. Injecting 25 ms/frame
#     of CPU work makes the rate go UP (7.7 -> 14.0 fps), because a busy page gets ticked more
#     often. frame_interval is therefore printed as a DIAGNOSTIC and must never be quoted as
#     frame time. What IS valid is the CPU work per frame — frame_cb and ScriptDuration.
#   * The renderer is measured at ~7.5 fps with ~130 ms idle between frames, so its data caches are
#     cold at every frame entry. A refactor whose whole point is cache locality (SoA) may therefore
#     look BETTER here than it would at a real frame rate. Treat a win here as necessary, not
#     sufficient.
#
# CACHE STATE IS PART OF THE RESULT. A cold fbtiles-cache changes these numbers massively, so the
# harness prints what it found and the baseline records it. Two runs at different cache states are
# not comparable and the file says which one it was.
set -uo pipefail
cd "$(dirname "$0")"

REPEATS="${1:-3}"
MEASURE_S="${2:-20}"
OUT="${3:-baseline.json}"
SIZE="${FB_BENCH_SIZE:-1280x720}"
WARMUP_S="${FB_BENCH_WARMUP:-120}"
WORK="${FB_BENCH_WORK:-/tmp/fb-bench}"

command -v node >/dev/null || { echo "no node on PATH"; exit 1; }
node -e "require('playwright')" 2>/dev/null || { echo "playwright not resolvable by node"; exit 1; }
export FB_CHROME="${FB_CHROME:-$HOME/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome}"
[ -x "$FB_CHROME" ] || { echo "no chrome at $FB_CHROME — set FB_CHROME"; exit 1; }
# Default :8080 is the LIVE stack, which anyone may rebuild under you mid-run — that already
# happened here and produced a "baseline of master" that was measuring someone else's refactor.
# For a baseline, pin a commit onto its own port with bench_stack.sh and point FB_BENCH_URL at it.
export FB_BENCH_URL="${FB_BENCH_URL:-http://localhost:8080}"
curl -s -f --max-time 3 "$FB_BENCH_URL/config.js" >/dev/null || {
    echo "nothing serving on $FB_BENCH_URL — see bench_stack.sh, or sim/run-podman.sh for :8080"; exit 1; }
WASM_SHA=$(curl -s "$FB_BENCH_URL/cc.wasm" | sha256sum | cut -d' ' -f1)
echo ">> target: $FB_BENCH_URL  wasm=${WASM_SHA:0:12}"
[ "$FB_BENCH_URL" = "http://localhost:8080" ] && echo "   NOTE: measuring the LIVE stack — it can be rebuilt underneath this run"

mkdir -p "$WORK"; rm -f "$WORK"/run-*.json "$WORK"/run-*.png
W="${SIZE%x*}"; H="${SIZE#*x}"

# Is the box quiet enough to measure CPU on? This gate exists because its absence already cost a
# whole afternoon of numbers: four runs were taken on a 4-core box while another agent ran podman
# builds and a stray fluidsynth held a full core for 51 minutes — 94% user, 1% idle. Those numbers
# were not the renderer's cost, they were the queue in front of it. Wall time inside the frame
# callback counts time descheduled, so contention inflates exactly the metric this harness sells.
idle_pct() {   # box-wide idle over 2 s, as a percentage of all cores
    python3 - <<'EOF'
import time
def snap():
    v = [int(x) for x in open('/proc/stat').readline().split()[1:]]
    return sum(v), v[3] + v[4]          # total, idle+iowait
t0, i0 = snap(); time.sleep(2); t1, i1 = snap()
print(int(100 * (i1 - i0) / max(1, t1 - t0)))
EOF
}
IDLE=$(idle_pct)
NPROC=$(nproc)
echo ">> box: ${NPROC} cores, ${IDLE}% idle"
if [ "$IDLE" -lt 40 ]; then
    echo "   TOP CPU (not necessarily ours):"
    ps -eo pcpu,comm --sort=-pcpu | head -4 | sed 's/^/     /'
fi
# Below ~25% idle on 4 cores there is not a free core for the browser, and the run measures
# contention. Refuse rather than print a confident number about the wrong thing.
if [ "$IDLE" -lt 25 ] && [ "${FB_BENCH_FORCE:-0}" != "1" ]; then
    echo "   FATAL: box is saturated (${IDLE}% idle) — a CPU baseline taken now measures the load,"
    echo "          not the renderer. Wait for it to go quiet, or set FB_BENCH_FORCE=1 to override"
    echo "          (the result is then not a baseline and must not be compared against one)."
    exit 1
fi
[ "$IDLE" -lt 40 ] && echo "   WARNING: only ${IDLE}% idle — treat the noise floor below as inflated"

# Cache warmth, straight off the volume — no container start needed to answer a question about a
# directory. Recorded in the baseline because it is the single biggest lever on these numbers.
VOL="$HOME/.local/share/containers/storage/volumes/fbtiles-cache/_data"
if [ -d "$VOL" ]; then
    CACHE_FILES=$(find "$VOL" -type f 2>/dev/null | wc -l)
    CACHE_SIZE=$(du -sh "$VOL" 2>/dev/null | cut -f1)
else
    CACHE_FILES=0; CACHE_SIZE="unknown"
fi
echo ">> tile cache: $CACHE_FILES files, $CACHE_SIZE  ($VOL)"
[ "$CACHE_FILES" -gt 1000 ] || echo "   WARNING: cache looks COLD — these numbers are not a warm baseline"

run() {  # run GROUND OUT_TAG [SYNTH_MS] [MEASURE_S_OVERRIDE]
    local ground="$1" tag="$2" synth="${3:-0}" msecs="${4:-$MEASURE_S}"
    # fb-aircraft is deliberately SHARED with the live stack (it is the scene's source, not the
    # code under test), and run-podman.sh restarts all three containers. A restart mid-window
    # resets the aircraft's pose, altitude and flight phase, which changes how many chunks are
    # drawn and how much work there is: that is a step change, not noise. Nobody can be asked to
    # not rebuild for 15 minutes -- so record it and let the report throw the run out.
    local ac0 ac1
    ac0=$(podman inspect -f '{{.State.StartedAt}}' "${FB_BENCH_AIRCRAFT:-fb-aircraft}" 2>/dev/null)
    node bench.js "$WORK/run-$tag.json" "$WORK/run-$tag.png" \
         "$ground" "$W" "$H" "$WARMUP_S" "$msecs" "$synth"
    local rc=$?
    [ $rc -eq 0 ] || { echo "   run $tag: NOT READY (rc=$rc) — not a usable sample"; return $rc; }
    # Second, independent opinion on the same pixels: a fast benchmark of an empty world is the
    # failure mode this whole file is built to avoid.
    local pct
    pct=$(python3 pngstat.py "$WORK/run-$tag.png" "$tag" | sed -n 's/.*, \([0-9]*\)% ground/\1/p')
    # Idle sampled right AFTER the window, not only once before the whole suite: another agent can
    # start a podman build halfway through and quietly turn the rest of the runs into a load
    # measurement. The report refuses to average runs whose box was busy.
    ac1=$(podman inspect -f '{{.State.StartedAt}}' "${FB_BENCH_AIRCRAFT:-fb-aircraft}" 2>/dev/null)
    python3 - "$WORK/run-$tag.json" "${pct:-0}" "$(idle_pct)" "$ac0" "$ac1" <<'EOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
# ground_pct is DIAGNOSTIC ONLY and must not become a threshold: it tracks the aircraft's pose at
# shutter time, not the build. Measured across builds it overlaps completely (master 38/26/25 %,
# a refactor 30/22/26 %), so a "47 % ground" gate would have failed master itself. pngstat's own
# verdict plus "chunks drawn > 0, 0 pending" are the real gates.
d['ground_pct'] = float(sys.argv[2])
d['idle_pct_after'] = int(sys.argv[3])
d['aircraft_restarted'] = (sys.argv[4] != sys.argv[5])
json.dump(d, open(p, 'w'))
EOF
    return 0
}

# One discarded run per ground. fb-tiles has in-memory caches of its own that are empty after a
# container restart even when the disk cache is warm, so run 1 is systematically different from
# runs 2..N. Measuring it would widen the noise floor with a transient that never repeats.
echo ">> warm-up (discarded)"
for g in osm photo; do run "$g" "warm-$g" 0 5 >/dev/null 2>&1; done

echo ">> measuring: $REPEATS x ${MEASURE_S}s x {osm,photo} @ $SIZE"
FAIL=0
for g in osm photo; do
    for i in $(seq 1 "$REPEATS"); do
        echo "-- $g run $i/$REPEATS"
        run "$g" "$g-$i" || FAIL=1
    done
done

# ---------------------------------------------------------------------------
# Self-test. Before believing a checker, watch it fail — pngstat.py and shot.sh both once reported
# confidently about the wrong thing, so a harness here does not get taken on trust.
#
# Inject 25 ms of CPU work per frame from OUTSIDE the renderer and require the CPU metrics to
# notice. Asserted on ScriptDuration, NOT on the frame interval: the interval moves the WRONG WAY
# under load (see the header), so a self-test built on it would have "passed" on a lie.
echo ">> self-test: 25 ms/frame of injected load must show up in the CPU metrics"
run osm "selftest" 25 >/dev/null 2>&1
python3 - "$WORK/run-osm-1.json" "$WORK/run-selftest.json" <<'EOF'
import json, sys
base = json.load(open(sys.argv[1]))
load = json.load(open(sys.argv[2]))
b, l = base['cdp']['ScriptDuration'], load['cdp']['ScriptDuration']
print(f"   ScriptDuration over the window: {b:.3f}s clean -> {l:.3f}s under load ({l/max(b,1e-9):.1f}x)")
print(f"   frame_interval p50 (the metric we do NOT trust): "
      f"{sorted(base['interval_ms'])[len(base['interval_ms'])//2]:.0f}ms -> "
      f"{sorted(load['interval_ms'])[len(load['interval_ms'])//2]:.0f}ms")
# The injected burn is many times the renderer's own ~0.5 ms/frame, so a harness that sees CPU work
# must show a large multiple here. 3x is far below the ~28x measured and still far above noise.
if l < b * 3:
    print("   SELF-TEST FAILED — the harness does not see injected CPU load. Do not trust it.")
    sys.exit(1)
print("   self-test passed — the CPU measurement responds to real work")
EOF
[ $? -eq 0 ] || FAIL=1

echo ">> report"
python3 bench_report.py "$OUT" "$WORK"/run-osm-[0-9]*.json "$WORK"/run-photo-[0-9]*.json || FAIL=1

python3 - "$OUT" "$CACHE_FILES" "$CACHE_SIZE" "$SIZE" "$MEASURE_S" "$FB_BENCH_URL" "$WASM_SHA" \
        "${FB_BENCH_REF:-unpinned}" "$IDLE" "$NPROC" <<'EOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d['env'] = {
    'cache_files': int(sys.argv[2]), 'cache_size': sys.argv[3], 'cache_state': 'warm',
    'viewport': sys.argv[4], 'measure_s': int(sys.argv[5]),
    'url': sys.argv[6],
    # Recorded, not assumed: two runs at different box loads are not comparable, and this is the
    # field that says so afterwards instead of everyone guessing.
    'idle_pct_at_start': int(sys.argv[9]), 'cores': int(sys.argv[10]),
    # The identity of what was measured, taken from the SERVER, not from the git working tree --
    # the tree is a different question, and answering it instead is how the first attempt at this
    # baseline ended up describing someone else's build.
    'wasm_sha256': sys.argv[7], 'ref': sys.argv[8],
    'gl': 'SwiftShader (software) — headless chromium, no GPU on this box',
    'not_measured': [
        'GPU cost — gl.finish() is ~0 ms here; software raster, no GPU on this box',
        'frame rate / frame time — headless paces an idle page at ~131 ms; adding CPU load makes '
        'the rate go UP. frame_interval is a diagnostic only.',
        'cache-locality realism — frames are ~130 ms apart, so caches are cold at frame entry',
    ],
}
json.dump(d, open(p, 'w'), indent=1)
EOF

exit $FAIL
