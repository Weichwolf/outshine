#!/usr/bin/env bash
# Serve the command center BUILT FROM A PINNED COMMIT on its own port, so a benchmark measures the
# artifact it names instead of whatever happens to be on :8080.
#
#   ./bench_stack.sh up <git-ref> [port]   -> builds ref's WASM, serves it, prints the URL
#   ./bench_stack.sh down                  -> removes the container
#
# WHY THIS EXISTS. A baseline was taken here against :8080 while another agent rebuilt that stack
# from their working tree; the numbers were real, reproducible, and about the wrong build. This is
# the same disease run-tests.sh already had (it published :8080, the port a live stack held, and
# measured the LIVE aircraft while printing its own model names) — so this follows run-tests.sh's
# cure: own name, own port, never the default.
#
# The stack gets its OWN aircraft as well as its own flightbox. Two reasons, both learned here:
#
#  1. It does not work otherwise. The aircraft pushes telemetry to ONE address (FLIGHTBOX_ADDR), so
#     a second flightbox on another port receives nothing, the camera never gets a pose, the world
#     never streams, and the benchmark measures an empty scene at 5 draw calls per frame while
#     reporting "NOT READY" in a line nobody reads. That happened here.
#  2. It removes the scene confound. run-podman.sh restarts all three containers, so a rebuild by
#     someone else mid-window would reset pose, altitude and flight phase — a step change in how
#     much there is to draw, dressed up as noise.
#
# fb-tiles IS shared, deliberately: it is a read-mostly cache of the real world, and sharing keeps
# the 600 MB tile cache warm for both. It serves data, it does not drive the scene.
set -uo pipefail
cd "$(dirname "$0")"
REPO="$(git rev-parse --show-toplevel)"

NAME=fb-bench-flightbox
AC=fb-bench-aircraft
NET=flightboxnet
WT=/tmp/fb-bench-src

case "${1:-}" in
up)
    REF="${2:?usage: bench_stack.sh up <git-ref> [port]}"
    PORT="${3:-8098}"
    SHA=$(git rev-parse --short "$REF") || exit 1

    # A worktree, not a checkout: the live tree belongs to whoever else is editing it right now,
    # and a benchmark must never make them stash their work to get its number.
    rm -rf "$WT"; git worktree prune
    git worktree add --detach "$WT" "$REF" >/dev/null || exit 1

    echo ">> building WASM from $REF ($SHA)"
    ( cd "$WT/sim" && ./build-wasm.sh ) >/tmp/fb-bench-build.log 2>&1 || {
        echo "   FATAL: build-wasm.sh failed — see /tmp/fb-bench-build.log"; tail -5 /tmp/fb-bench-build.log; exit 1; }

    echo ">> building image"
    podman build -q -f "$WT/sim/flightbox/Containerfile" -t "$NAME:$SHA" "$WT/sim" >/dev/null || {
        echo "   FATAL: image build failed"; exit 1; }

    podman rm -f "$NAME" "$AC" >/dev/null 2>&1
    podman network exists "$NET" || podman network create "$NET" >/dev/null
    podman run -d --name "$NAME" --network "$NET" \
        -e AIRCRAFT_ADDR="$AC" -e TILES_URL="http://localhost:8081" \
        -p "$PORT":8080 "$NAME:$SHA" >/dev/null || {
        echo "   FATAL: $NAME did not start (port $PORT taken?)"; exit 1; }
    # Our own aircraft, pointed at OUR flightbox. Reuses the existing fb-aircraft image: the
    # firmware and FDM are not what the renderer benchmark varies, and rebuilding iNav per run
    # would cost minutes to change nothing. It shares the live fb-tiles for ground elevation.
    podman image exists fb-aircraft || { echo "   FATAL: no fb-aircraft image — run sim/run-podman.sh once"; exit 1; }
    podman run -d --name "$AC" --network "$NET" \
        -e FLIGHTBOX_ADDR="$NAME" -e TILES_ADDR="fb-tiles:8081" \
        -e ORIGIN_LAT=52.045 -e ORIGIN_LON=9.385 -e FDM_MODEL=1 fb-aircraft >/dev/null || {
        echo "   FATAL: $AC did not start"; exit 1; }

    for i in $(seq 1 30); do
        curl -s -f --max-time 2 "http://localhost:$PORT/config.js" >/dev/null && break
        sleep 1
    done
    curl -s -f --max-time 2 "http://localhost:$PORT/config.js" >/dev/null || {
        echo "   FATAL: nothing serving on :$PORT after 30s"; podman logs "$NAME" | tail -5; exit 1; }

    echo ">> $REF ($SHA) on http://localhost:$PORT"
    echo "   wasm sha256: $(curl -s "http://localhost:$PORT/cc.wasm" | sha256sum | cut -d' ' -f1)"
    ;;
down)
    podman rm -f "$NAME" "$AC" >/dev/null 2>&1
    rm -rf "$WT"; git worktree prune
    echo ">> $NAME removed"
    ;;
*)
    sed -n '2,8p' "$0"; exit 1 ;;
esac
