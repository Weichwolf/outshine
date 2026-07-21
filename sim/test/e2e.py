#!/usr/bin/env python3
"""Headless mission harness — pure runner, no hard-coded scenario. Loads a mission file that defines
BOTH the flight (takeoff airport/runway, waypoints AGL, landing) AND its pass/abort conditions,
resolves it (runway thresholds + heading from the OurAirports DB, AGL->home-relative via the DEM in
mission.py), spawns the aircraft container at the takeoff threshold aligned with the runway, acts as
the thin Command Center over MSP (arm -> upload waypoints -> NAV WP, gear per the mission), and
evaluates the mission's OWN success/abort criteria against iNav telemetry. Everything scenario-specific
lives in the mission JSON, so the same files drive these tests and (later) CC flight-training.

iNav flies natively; the CC only commands. Sim runs on the FB_TIME_SCALE clock; with the shim preloaded
this process shares it (time.monotonic), so a run is deterministic and N x faster than real time.

Usage: e2e.py <mission|aircraft> [sim_seconds]   env: MOUNT_EEPROM, FB_TIME_SCALE, TILES_ADDR
"""
import socket, struct, subprocess, sys, threading, time, math, os, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mission as MISSION
import ringtrack as RT

ROOT = Path(__file__).resolve().parents[1]
TILES = os.environ.get("TILES_HOST", "http://localhost:8081")   # host-reachable DEM (container uses fb-tiles:8081)

def load_mission(arg):
    p = Path(arg)
    if not p.exists():
        p = ROOT / "missions" / (arg if arg.endswith(".json") else f"{arg}.json")
    m = json.loads(p.read_text())
    r = MISSION.resolve(m, tiles_url=TILES)
    m["_resolved"] = r
    return m

def crc8(c,b):
    c^=b
    for _ in range(8): c=((c<<1)^0xD5)&0xff if c&0x80 else (c<<1)&0xff
    return c

class MSP:
    def __init__(s,h,p):
        s.h=h; s.p=p; s._open()
    def _open(s):
        s.f=socket.create_connection((s.h,s.p),timeout=3); s.f.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1); s.f.settimeout(0.4)
    def send(s,cmd,pl=b""):
        h=bytes([0x24,0x58,0x3C,0,cmd&0xff,cmd>>8,len(pl)&0xff,len(pl)>>8]); c=0
        for b in h[3:]+pl: c=crc8(c,b)
        try: s.f.sendall(h+pl+bytes([c]))
        except OSError:                      # iNav closed the socket (overload/idle) -> reconnect, don't die
            s._open(); s.f.sendall(h+pl+bytes([c]))
    def recv(s):
        st=0;n=0;got=0;cmd=0;pl=bytearray()
        try:
            for _ in range(4096):
                d=s.f.recv(1)
                if not d: return None,None
                b=d[0]
                if st==0: st=1 if b==0x24 else 0
                elif st==1: st=2 if b==0x58 else 0
                elif st==2: st=3 if b in (0x3E,0x21) else 0
                elif st==3: st=4
                elif st==4: cmd=b;st=5
                elif st==5: cmd|=b<<8;st=6
                elif st==6: n=b;st=7
                elif st==7: n|=b<<8;got=0;st=8 if n else 9
                elif st==8:
                    pl.append(b);got+=1
                    if got==n: st=9
                elif st==9: return cmd,bytes(pl)
        except socket.timeout: return None,None
        return None,None
    def req(s,cmd,pl=b""):
        s.send(cmd,pl)
        for _ in range(30):
            c,p=s.recv()
            if c==cmd: return p
            if c is None: return None

def cc_thread(port, wps, gear_retract, angle_hold_alt, speedbrake, stop, state, climb_pitch=1500, land_wp=None):
    """thin CC: arm (edge + YAW_HI nav bypass) -> wings-level ANGLE climb to a target ALTITUDE -> NAV WP.
    iNav flies natively. The climb-out is gated on altitude (read from MSP), NOT elapsed time, so a
    marginal airframe always reaches a safe height before NAV's first turn regardless of the run-to-run
    timing jitter of the async CC<->iNav link — the determinism-robustness that makes all three repeatable."""
    m=MSP("127.0.0.1",port); t0=time.monotonic(); cal_done=-1; up=False; established=False
    while not stop.is_set():
        ts=time.monotonic()-t0
        st=m.req(0x2000); af=struct.unpack("<I",st[9:13])[0] if st and len(st)>=13 else 0
        armed=bool(af&0x4); state['armed']=armed
        al=m.req(109); agl=(struct.unpack("<i",al[0:4])[0]/100.0) if al and len(al)>=4 else 0   # MSP_ALTITUDE, home-rel m
        rc=[1500,1500,1000,1500,1000,1000,1000,1500,1000,1000] # roll pitch thr yaw AUX1..4 AUX5(gear) AUX6(speedbrake)
        if cal_done<0 and not (af&(1<<9)) and ts>0.5: cal_done=ts
        if cal_done<0: pass
        elif not armed:
            if (ts-cal_done)%3.0>=1.5: rc[4]=2000; rc[3]=2000    # ARM + YAW_HI (bypass NAV_UNSAFE)
        else:
            if not up:
                # nav waypoints (action 1) + optional final LAND waypoint (action 8 -> iNav descends and
                # lands at the runway threshold; NAV_WP_ACTION_LAND switches to WAYPOINT_RTH_LAND natively).
                seq=[(w['lat'],w['lon'],w['alt_rel'],1) for w in wps]
                if land_wp: seq.append((land_wp['lat'],land_wp['lon'],land_wp['alt_rel'],8))
                for i,(la,lo,ar,act) in enumerate(seq):
                    b=bytearray(21); b[0]=i+1; b[1]=act
                    b[2:6]=struct.pack("<i",int(la*1e7)); b[6:10]=struct.pack("<i",int(lo*1e7))
                    b[10:14]=struct.pack("<i",int(round(ar*100))); b[20]=0xa5 if i==len(seq)-1 else 0
                    m.send(209,bytes(b))
                up=True
            if agl>=angle_hold_alt: established=True             # latch: once at safe altitude, stay in NAV
            if angle_hold_alt>0 and not established:            # wings-level ANGLE climb-out (pitch-up stick +
                rc[4]=2000; rc[5]=2000; rc[2]=1600; rc[1]=climb_pitch   # climb throttle) until AGL reaches target;
                if gear_retract: rc[8]=2000                    # a fast jet climbs the WHOLE way in stable ANGLE so
                if speedbrake:   rc[9]=2000                    # NAV enters already at altitude -> no zoom overshoot
            else:
                rc[4]=2000; rc[7]=2000                          # ARM + NAV WP -> iNav flies natively
                if gear_retract: rc[8]=2000                     # AUX5 high -> retract gear (AUXMAP=gear)
                if speedbrake:   rc[9]=2000                     # AUX6 high -> deploy speedbrake (slow a fast jet)
        m.send(200,struct.pack("<10H",*rc))
        time.sleep(0.033)

def latest_flt():
    r=subprocess.run("podman logs --tail 40 fb-aircraft 2>&1",shell=True,capture_output=True,text=True)
    for ln in reversed(r.stdout.splitlines()):
        if ln.startswith("[flt]"):
            try:
                lat,lon=map(float,ln.split()[2].split(","))
                agl=float(ln.split("alt")[1].split("g")[0])
                return lat,lon,agl,("nan" in ln)
            except: return None
    return None

def dist(a,b,c,d): return math.hypot((c-a)*111320,(d-b)*111320*math.cos(math.radians(a)))

def read_trajectory():
    """FULL flight path as [(lat,lon,alt_asl,is_nan)] from every [flt] line — the real 3D track, not just
    the latest sample, so a fast fly-through between coarse polls isn't missed."""
    r=subprocess.run("podman logs fb-aircraft 2>&1",shell=True,capture_output=True,text=True)
    tj=[]
    for ln in r.stdout.splitlines():
        if not ln.startswith("[flt]"): continue
        try:
            lat,lon=map(float,ln.split()[2].split(","))
            asl=float(ln.split("alt")[1].split("a")[0].split("/")[1])   # ".../<asl>a"
            tj.append((lat,lon,asl,"nan" in ln))
        except: pass
    return tj

def main():
    arg = sys.argv[1] if len(sys.argv)>1 else "c172"
    M = load_mission(arg); R = M["_resolved"]
    ac = R["aircraft"]; to = R["takeoff"]; wps = R["waypoints"]
    succ = M.get("success", {}); abort = M.get("abort", {})
    secs = float(sys.argv[2]) if len(sys.argv)>2 else abort.get("timeout_s", 220)   # run for the mission's own timeout
    cap = succ.get("capture_radius_m", 200); to_agl = succ.get("takeoff_agl_m", 50)
    max_agl = abort.get("max_agl_m", 3000); on_nan = abort.get("on_nan", True)
    gear = M.get("procedure", {}).get("gear_retract", False)
    angle_hold_alt = M.get("procedure", {}).get("angle_hold_alt", 0)   # climb in ANGLE to this AGL before NAV
    speedbrake = M.get("procedure", {}).get("speedbrake", False)
    climb_pitch = M.get("procedure", {}).get("angle_climb_pitch", 1500)   # RC pitch during ANGLE climb (>1500 = nose-up)
    if any(w.get("alt_rel") is None for w in wps):
        print(f"FATAL: mission WPs unresolved (DEM/tiles at {TILES} unavailable)"); sys.exit(2)
    # The mission is a DENSE directed ring track computed from iNav's own nav math (climb-out at
    # nav_fw_climb_angle, a Catmull-Rom spline through the WPs rounded at the coordinated turn radius, an
    # approach glideslope aligned with the landing runway) — >=100 rings. Flying the mission = threading the
    # hoops in sequence from the correct side; clipping a 2D sphere near a point is NOT enough. ringtrack.py
    # generates the identical track for the CC route render, so the user watches the plane thread these gates.
    ring_r = succ.get("ring_radius_m", None)
    rings = RT.build_track(R, ac, **({"ring_radius": ring_r} if ring_r else {}))
    pass_frac = succ.get("ring_pass_frac", 0.90)                           # in-order fraction required to PASS
    need_rings = math.ceil(len(rings)*pass_frac)
    print(f"== mission '{M.get('name',arg)}' :: {ac} from {to['icao']}/{to['runway']} "
          f"(hdg {to['heading_deg']:.0f}), {len(wps)} WPs, {len(rings)} rings (r={rings[0]['r']:.0f}m), "
          f"need {need_rings} in-order ==")

    # Time-scale must be set at the SHELL before this process starts: the clock shim (LD_PRELOAD) caches
    # FB_TIME_SCALE at init and also scales THIS host process's clock, so it can't be re-capped here without
    # desyncing the host CC-thread from the container sim. The F-16's 1 kHz FLCS hits iNav's
    # SYSTEM_OVERLOADED arming block above ~1x under CPU contention -> run the f16 at FB_TIME_SCALE=1
    # (mission-test.sh enforces this per aircraft). Warn if it was launched too fast.
    ts=float(os.environ.get("FB_TIME_SCALE","1"))
    if ac=="f16" and ts>1: print(f"[e2e] WARN f16 at {ts:g}x: 1 kHz FLCS may trip SYSTEM_OVERLOADED (non-deterministic); use FB_TIME_SCALE=1", flush=True)
    subprocess.run("podman rm -f fb-aircraft",shell=True,capture_output=True)
    mdir=str(ROOT/"aircraft"/"models"/ac)
    mounts=f" -v {mdir}/profile.env:/app/models/{ac}/profile.env:ro"      # live profile (SPAWN_SPEED/FBW/AUXMAP), no rebuild
    if os.path.exists(f"{mdir}/{ac}.xml"): mounts+=f" -v {mdir}/{ac}.xml:/app/models/{ac}/{ac}.xml:ro"   # live JSBSim model (FLCS/FBW tweaks), no rebuild
    if os.environ.get("MOUNT_EEPROM"): mounts+=f" -v {os.environ['MOUNT_EEPROM']}:/app/models/{ac}/eeprom.bin"
    subprocess.run(f"podman run -d --name fb-aircraft --network flightboxnet -p 5761:5761 {mounts} "
       f"-e AIRCRAFT={ac} -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 "
       f"-e FB_TIME_SCALE={ts:g} -e FLT_LOG_S={os.environ.get('FLT_LOG_S','1')} "  # 1s track: finer than the ring pitch so pierces aren't missed
       f"-e ORIGIN_LAT={to['lat']} -e ORIGIN_LON={to['lon']} -e ORIGIN_HDG={to['heading_deg']} fb-aircraft",
       shell=True,capture_output=True)
    # wait for iNav to actually answer MSP (robust at any FB_TIME_SCALE — a fixed sleep would be scaled
    # by the preloaded clock shim and fire before the container has booted).
    for _ in range(400):
        try:
            probe=MSP("127.0.0.1",5761); probe.req(0x2000); probe.f.close(); break
        except OSError: time.sleep(0.25)
    stop=threading.Event(); state={}
    ld=R["land"]; land_wp={"lat":ld["lat"],"lon":ld["lon"],"alt_rel":ld["elev_m"]-to["elev_m"]}   # threshold, home-relative
    threading.Thread(target=cc_thread,args=(5761,wps,gear,angle_hold_alt,speedbrake,stop,state,climb_pitch,land_wp),daemon=True).start()

    hold = bool(os.environ.get("FB_HOLD"))   # keep flying (loiter last WP) until timeout + leave the container up,
                                              # so the live CC / Playwright agents have a persistent telemetry target
    armed=took=crashed=over=False; threaded=inorder=0; peak=0; announced=False; t0=time.monotonic()
    while time.monotonic()-t0<secs:
        time.sleep(3)
        s=latest_flt()
        if not s: continue
        lat,lon,agl,nan=s
        if state.get('armed'): armed=True
        if agl>to_agl: took=True
        peak=max(peak,agl)
        if nan and took: crashed=True; break
        if agl>max_agl: over=True; break
        tj3=[(a,b,c) for (a,b,c,_) in read_trajectory()]      # the FULL 3D path so far (lat,lon,asl)
        threaded,inorder = RT.score(tj3, rings)
        if inorder>=need_rings:
            if not hold: break
            if not announced: print(f"== {ac} == {inorder}/{len(rings)} rings in sequence, HOLDING for the live CC", flush=True); announced=True
    stop.set()
    if not hold:
        subprocess.run("podman stop -t 2 fb-aircraft",shell=True,capture_output=True)

    ok_arm=armed; ok_to=took; ok_rings=inorder>=need_rings
    abrt = ("CRASH(NaN)" if (crashed and on_nan) else "OVER-CLIMB" if over else None)
    verdict = "PASS" if (ok_arm and ok_to and ok_rings and not abrt) else "FAIL"
    print(f"== {ac} == ARM={'OK' if ok_arm else 'FAIL'} TAKEOFF={'OK' if ok_to else f'FAIL(peak {peak:.0f})'} "
          f"RINGS={inorder}/{len(rings)} in-order ({threaded} threaded, need {need_rings})"
          f"{' ABORT:'+abrt if abrt else ''} -> {verdict}")
    sys.exit(0 if verdict=="PASS" else 1)

if __name__ == "__main__":   # importable (MSP/crc8/spawn) by cc_loiter.py + the PPL fixture harness
    main()
