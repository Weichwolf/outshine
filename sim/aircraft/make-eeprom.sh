#!/usr/bin/env bash
# Regenerate eeprom.bin reproducibly from inav-config.txt by applying the CLI to a fresh
# iNav SITL and saving. The SITL scheduler (and thus its CLI) only runs once X-Plane is
# connected, so we run xp_bridge as a pure sensor responder (XP_NOMSP=1) to satisfy that,
# which also leaves the MSP/CLI port (5760) free. Run from sim/aircraft/ (needs podman + python3).
set -euo pipefail
cd "$(dirname "$0")"
IMG=fb-aircraft
CN=eeprom-gen
PORT=5762

podman rm -f "$CN" >/dev/null 2>&1 || true
podman run -d --name "$CN" -p ${PORT}:5760 --entrypoint /bin/sh "$IMG" -c '
  rm -f /app/eeprom.bin
  /app/SITL.elf --path=/app/eeprom.bin --sim=xp --simip=127.0.0.1 --simport=49000 \
      --chanmap=M01-01,S01-02,S02-03,S03-04 > /tmp/sitl.log 2>&1 &
  sleep 1
  XP_NOMSP=1 /usr/local/bin/xp_bridge > /tmp/bridge.log 2>&1
' >/dev/null

# wait until SITL reaches CONNECTED (scheduler + CLI up)
for i in $(seq 1 20); do
  podman exec "$CN" grep -q "successfully established" /tmp/sitl.log 2>/dev/null && break
  sleep 1
done
sleep 2

python3 - "$PORT" <<'PY'
import socket, sys, time
port = int(sys.argv[1])
lines = []
for raw in open("inav-config.txt"):
    s = raw.split("#")[0].split(";")[0].strip()
    if s and s.lower() != "save":
        lines.append(s)
s = socket.create_connection(("127.0.0.1", port), timeout=5); s.settimeout(0.4)
def drain():
    try:
        while True:
            if not s.recv(4096): break
    except socket.timeout: pass
s.sendall(b"#\r\n"); time.sleep(0.3); drain()
for ln in lines:
    s.sendall((ln + "\r\n").encode()); time.sleep(0.03); drain()
try:
    s.sendall(b"save\r\n")      # persists eeprom + reboots (connection drops -> ignore)
    time.sleep(1.0); drain()
except OSError: pass
print("applied %d config lines + save" % len(lines))
PY

sleep 3
podman cp "$CN":/app/eeprom.bin ./eeprom.bin
podman rm -f "$CN" >/dev/null 2>&1 || true
echo "eeprom.bin regenerated ($(stat -c%s eeprom.bin) bytes)"
