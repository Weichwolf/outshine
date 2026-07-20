#!/usr/bin/env python3
"""Default demo mission: fly a continuous circle over a point so the WASM Command Center (localhost:8080)
always has something live to watch. Spawns the aircraft AIRBORNE at the loiter altitude, arms it, and
uploads a ring of waypoints closed by a native iNav JUMP waypoint (infinite repeat) — so iNav itself
circles forever, no re-upload. Reuses e2e.py's MSP + container spawn.

  test/cc_loiter.py [aircraft] [lat] [lon] [alt_m] [radius_m]     defaults: c172 Hameln 500 m / 2000 m

Pair with flightbox at the same origin (test/cc_loiter.sh does both).
"""
import os, sys, math, time, struct, threading, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import MSP                                   # reuse the MSP wire client + framing

ROOT = Path(__file__).resolve().parents[1]
AC     = sys.argv[1] if len(sys.argv) > 1 else "c172"
LAT    = float(sys.argv[2]) if len(sys.argv) > 2 else 52.045     # Hameln
LON    = float(sys.argv[3]) if len(sys.argv) > 3 else 9.385
ALT    = float(sys.argv[4]) if len(sys.argv) > 4 else 500.0      # loiter altitude AGL
RADIUS = float(sys.argv[5]) if len(sys.argv) > 5 else 2000.0     # circle radius
N      = 12                                                       # waypoints around the ring
SECS   = float(os.environ.get("LOITER_SECS", "100000"))          # run ~forever by default

CRUISE = {"c172": 22.0, "sgs233": 18.0, "f16": 55.0}.get(AC, 22.0)

def ring():
    """N waypoints on the circle of RADIUS around (LAT,LON). No JUMP waypoint — iNav's JUMP left the
    mission in an invalid end-state (nav_state oscillated NONE<->WP, the aircraft loitered a single WP
    off-centre and drifted). Instead the runner re-uploads this ring when the lap finishes (below),
    which restarts NAV cleanly at WP1 -> a continuous, centred circle."""
    wps = []
    for i in range(N):
        a = 2*math.pi*i/N
        dlat = RADIUS*math.cos(a)/111320.0
        dlon = RADIUS*math.sin(a)/(111320.0*math.cos(math.radians(LAT)))
        wps.append((LAT+dlat, LON+dlon, 0.0, 1, 0, 0))            # action 1 = WAYPOINT, alt_rel 0 (= arm altitude)
    return wps

def nav_status(m):
    """MSP_NAV_STATUS -> (current_wp, nav_state). nav_state: 0 none, 1/2 RTH, 3/4 hold, >=5 WP enroute."""
    p = m.req(121)
    if p and len(p) >= 5: return p[3], p[1]
    return 0, -1

def spawn():
    subprocess.run("podman rm -f fb-aircraft", shell=True, capture_output=True)
    mdir = ROOT/"aircraft"/"models"/AC
    mounts = f" -v {mdir}/profile.env:/app/models/{AC}/profile.env:ro"
    if (mdir/f"{AC}.xml").exists(): mounts += f" -v {mdir}/{AC}.xml:/app/models/{AC}/{AC}.xml:ro"
    if os.environ.get("MOUNT_EEPROM"): mounts += f" -v {os.environ['MOUNT_EEPROM']}:/app/models/{AC}/eeprom.bin"
    subprocess.run(
        f"podman run -d --name fb-aircraft --network flightboxnet -p 5761:5761 {mounts} "
        f"-e AIRCRAFT={AC} -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 "
        f"-e FB_TIME_SCALE={os.environ.get('FB_TIME_SCALE','1')} -e FLT_LOG_S=5 "
        f"-e ORIGIN_LAT={LAT} -e ORIGIN_LON={LON} -e ORIGIN_HDG=90 "
        f"-e SPAWN_ALT={ALT:.0f} -e SPAWN_SPEED={CRUISE:.0f} fb-aircraft",
        shell=True, capture_output=True)

def send_wp(m, i, w):
    lat, lon, alt, act, p1, p2 = w
    b = bytearray(21); b[0] = i+1; b[1] = act
    b[2:6]  = struct.pack("<i", int(lat*1e7)); b[6:10] = struct.pack("<i", int(lon*1e7))
    b[10:14]= struct.pack("<i", int(round(alt*100)))
    b[14:16]= struct.pack("<h", p1)
    b[16:18]= struct.pack("<h", p2)
    b[20]   = 0xa5 if i == N-1 else 0                            # last-WP flag on the final ring point
    m.send(209, bytes(b))

def main():
    print(f"[loiter] {AC} circling {RADIUS:.0f} m @ {ALT:.0f} m over {LAT:.4f},{LON:.4f} ({N} WPs, re-upload cycling)")
    spawn()
    def connect():
        for _ in range(400):
            try:
                p = MSP("127.0.0.1", 5761); p.req(0x2000); return p    # req, not just connect: SITL must ANSWER
            except OSError: time.sleep(0.25)
        raise SystemExit("iNav MSP never came up on 5761")
    m = connect(); wps = ring(); t0 = time.monotonic()
    cal_done = -1; up = False; hold_since = None; loops = 0; af = 0; armed = False
    while time.monotonic()-t0 < SECS:
        ts = time.monotonic()-t0; loops += 1
        # Blocking MSP reqs are kept OFF the RC hot path: iNav's MSP receiver fails safe (-> RTH) if RC
        # stops arriving, and a status req that stalls waiting for a reply used to create exactly that gap.
        # Poll arming ~6 Hz, nav-state ~1 Hz; SEND RC EVERY loop (~30 Hz, steady) so the link never drops.
        if loops % 5 == 0:
            try:
                st = m.req(0x2000); af = struct.unpack("<I", st[9:13])[0] if st and len(st) >= 13 else af
            except OSError:
                m = connect(); up = False; continue
            armed = bool(af & 0x4)
            if cal_done < 0 and not (af & (1<<9)) and ts > 0.5: cal_done = ts
        rc = [1500,1500,1000,1500,1000,1000,1000,1500,1000,1000]   # throttle LOW so iNav will arm; NAV WP then
                                                                    # sets its own cruise throttle once armed
        if cal_done < 0: pass
        elif not armed:
            if (ts-cal_done) % 3.0 >= 1.5: rc[4] = 2000; rc[3] = 2000       # ARM + YAW_HI (bypass NAV_UNSAFE)
        else:
            if not up:
                for i, w in enumerate(wps): send_wp(m, i, w)
                up = True; hold_since = None
            rc[4] = 2000; rc[7] = 2000                                       # ARM + NAV WP -> iNav circles natively
            if loops % 30 == 0:                                             # lap done (iNav holds last WP) ->
                wp, nst = nav_status(m)                                      # re-upload -> NAV restarts at WP1
                if nst in (3, 4):
                    hold_since = hold_since or ts
                    if ts - hold_since > 1.5: up = False
                else:
                    hold_since = None
        try:
            m.send(200, struct.pack("<10H", *rc))
        except OSError:
            m = connect(); up = False
        time.sleep(0.033)

if __name__ == "__main__":
    main()
