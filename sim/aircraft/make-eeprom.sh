#!/usr/bin/env bash
# Regenerate an eeprom reproducibly by applying the CLI to a fresh iNav SITL and saving.
#   make-eeprom.sh            -> inav-config.txt                        -> eeprom.bin (shared default)
#   make-eeprom.sh <aircraft> -> inav-config.txt + models/<ac>/inav.diff -> models/<ac>/eeprom.bin
# The per-aircraft overlay (inav.diff) carries only what differs (nav_fw throttle/bank/loiter per the
# airframe's speed) — one shared eeprom can't serve a 15 m/s glider and a 150 m/s jet. run.sh picks
# models/<ac>/eeprom.bin when present. The SITL scheduler/CLI only runs once X-Plane connects, so we
# run xp_bridge as a pure sensor responder (XP_NOMSP=1). Run from sim/aircraft/ (needs podman+python3).
set -euo pipefail
cd "$(dirname "$0")"
IMG=fb-aircraft
CN=eeprom-gen
PORT=5762
AC="${1:-}"
CFGS=(inav-config.txt)
OUT="eeprom.bin"
if [ -n "$AC" ]; then
  OUT="models/$AC/eeprom.bin"
  [ -f "models/$AC/inav.diff" ] && CFGS+=("models/$AC/inav.diff")
fi

podman rm -f "$CN" >/dev/null 2>&1 || true
# Stop any running aircraft SITL first: eeprom generation spins up its own SITL, and a second one
# flying in the background would be a second SITL.elf (CPU/timing contention that made this flaky).
# The aircraft must restart to pick up the new eeprom anyway, so removing it here costs nothing.
podman rm -f fb-aircraft >/dev/null 2>&1 || true
# Vanilla SITL WITHOUT --sim runs "Configurator only": its CLI/MSP is on 5760 immediately, no
# X-Plane responder needed. (The old flow booted --sim=xp + a sensor responder to bring the sim
# link up first; unnecessary and it referenced the now-removed bridge.)
podman run -d --name "$CN" -p ${PORT}:5760 --entrypoint /bin/sh "$IMG" -c '
  rm -f /app/eeprom.bin
  /app/SITL.elf --path=/app/eeprom.bin > /tmp/sitl.log 2>&1
' >/dev/null
sleep 3

# Apply the CLI + verify. The SITL CLI only answers once the sim link is up, and a fresh boot resets
# the first connections -> retry the whole exchange, and read back a sentinel so a silent connection
# failure can NEVER masquerade as a good eeprom (that bug made tuning changes invisible for a while).
python3 - "$PORT" "${CFGS[@]}" <<'PY'
import socket, sys, time
port = int(sys.argv[1])
lines = []
for cfg in sys.argv[2:]:
    for raw in open(cfg):
        s = raw.split("#")[0].split(";")[0].strip()
        if s and s.lower() != "save":
            lines.append(s)

def connect_cli(deadline):
    while time.time() < deadline:
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=3); s.settimeout(0.5)
            s.sendall(b"#\r\n"); time.sleep(0.4)
            buf = b""
            try:
                while True:
                    d = s.recv(4096)
                    if not d: break
                    buf += d
            except socket.timeout: pass
            if b"CLI" in buf or b"#" in buf:
                return s
            s.close()
        except OSError:
            time.sleep(1.0)
    return None

def cmd(s, c, wait=0.05):
    s.sendall((c + "\r\n").encode()); time.sleep(wait); out = b""
    try:
        while True:
            d = s.recv(4096)
            if not d: break
            out += d
    except socket.timeout: pass
    return out.decode(errors="replace")

s = connect_cli(time.time() + 40)
if not s:
    print("FATAL: SITL CLI never answered"); sys.exit(2)
bad = []
for ln in lines:
    r = cmd(s, ln)
    if any(k in r for k in ("Invalid", "Unknown", "###ERROR###", "not valid")):
        bad.append((ln, r.strip()[:70]))
if bad:
    for ln, r in bad: print("REJECTED:", ln, "->", r)
    print("FATAL: %d config line(s) rejected" % len(bad)); sys.exit(3)
# sentinel read-back BEFORE save proves the values took (last set line = the per-aircraft overlay)
setlines = [l for l in lines if l.startswith("set ")]
sentinel = setlines[-1] if setlines else None
if sentinel:
    key = sentinel.split("=")[0].replace("set", "").strip()
    want = sentinel.split("=")[1].strip()
    got = cmd(s, "get " + key)
    if want not in got:
        print("FATAL: sentinel %s expected %s, got: %s" % (key, want, got.strip()[:80])); sys.exit(4)
try:
    s.sendall(b"save\r\n"); time.sleep(1.2)
except OSError: pass
print("applied %d config lines + save (verified %s)" % (len(lines), sentinel and key or "-"))
PY

sleep 3
mkdir -p "$(dirname "$OUT")"
podman cp "$CN":/app/eeprom.bin "$OUT"
podman rm -f "$CN" >/dev/null 2>&1 || true
echo "$OUT regenerated ($(stat -c%s "$OUT") bytes)${AC:+ [aircraft: $AC]}"
