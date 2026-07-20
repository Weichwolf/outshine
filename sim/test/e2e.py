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
        s.f=socket.create_connection((h,p),timeout=3); s.f.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1); s.f.settimeout(0.4)
    def send(s,cmd,pl=b""):
        h=bytes([0x24,0x58,0x3C,0,cmd&0xff,cmd>>8,len(pl)&0xff,len(pl)>>8]); c=0
        for b in h[3:]+pl: c=crc8(c,b)
        s.f.sendall(h+pl+bytes([c]))
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

def cc_thread(port, wps, gear_retract, stop, state):
    """thin CC: arm (edge + YAW_HI nav bypass) -> upload waypoints -> NAV WP. iNav flies natively."""
    m=MSP("127.0.0.1",port); t0=time.monotonic(); cal_done=-1; up=False
    while not stop.is_set():
        ts=time.monotonic()-t0
        st=m.req(0x2000); af=struct.unpack("<I",st[9:13])[0] if st and len(st)>=13 else 0
        armed=bool(af&0x4); state['armed']=armed
        rc=[1500,1500,1000,1500,1000,1000,1000,1500,1000]     # roll pitch thr yaw AUX1..4 AUX5(gear:down)
        if cal_done<0 and not (af&(1<<9)) and ts>0.5: cal_done=ts
        if cal_done<0: pass
        elif not armed:
            if (ts-cal_done)%3.0>=1.5: rc[4]=2000; rc[3]=2000    # ARM + YAW_HI (bypass NAV_UNSAFE)
        else:
            if not up:
                for i,w in enumerate(wps):
                    b=bytearray(21); b[0]=i+1; b[1]=1
                    b[2:6]=struct.pack("<i",int(w['lat']*1e7)); b[6:10]=struct.pack("<i",int(w['lon']*1e7))
                    b[10:14]=struct.pack("<i",int(round(w['alt_rel']*100))); b[20]=0xa5 if i==len(wps)-1 else 0
                    m.send(209,bytes(b))
                up=True
            rc[4]=2000; rc[7]=2000                              # ARM + NAV WP -> iNav flies natively
            if gear_retract: rc[8]=2000                         # AUX5 high -> retract gear (AUXMAP=gear models)
        m.send(200,struct.pack("<9H",*rc))
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

def main():
    arg = sys.argv[1] if len(sys.argv)>1 else "c172"
    M = load_mission(arg); R = M["_resolved"]
    ac = R["aircraft"]; to = R["takeoff"]; wps = R["waypoints"]
    succ = M.get("success", {}); abort = M.get("abort", {})
    secs = float(sys.argv[2]) if len(sys.argv)>2 else abort.get("timeout_s", 220)   # run for the mission's own timeout
    cap = succ.get("capture_radius_m", 200); to_agl = succ.get("takeoff_agl_m", 50)
    max_agl = abort.get("max_agl_m", 3000); on_nan = abort.get("on_nan", True)
    gear = M.get("procedure", {}).get("gear_retract", False)
    if any(w.get("alt_rel") is None for w in wps):
        print(f"FATAL: mission WPs unresolved (DEM/tiles at {TILES} unavailable)"); sys.exit(2)
    print(f"== mission '{M.get('name',arg)}' :: {ac} from {to['icao']}/{to['runway']} "
          f"(hdg {to['heading_deg']:.0f}), {len(wps)} WPs, cap {cap:.0f}m ==")

    subprocess.run("podman rm -f fb-aircraft",shell=True,capture_output=True)
    mounts=f" -v {os.environ['MOUNT_EEPROM']}:/app/models/{ac}/eeprom.bin" if os.environ.get("MOUNT_EEPROM") else ""
    subprocess.run(f"podman run -d --name fb-aircraft --network flightboxnet -p 5761:5761 {mounts} "
       f"-e AIRCRAFT={ac} -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 "
       f"-e FB_TIME_SCALE={os.environ.get('FB_TIME_SCALE','1')} -e FLT_LOG_S=3 "
       f"-e ORIGIN_LAT={to['lat']} -e ORIGIN_LON={to['lon']} -e ORIGIN_HDG={to['heading_deg']} fb-aircraft",
       shell=True,capture_output=True)
    # wait for iNav to actually answer MSP (robust at any FB_TIME_SCALE — a fixed sleep would be scaled
    # by the preloaded clock shim and fire before the container has booted).
    for _ in range(400):
        try:
            probe=MSP("127.0.0.1",5761); probe.req(0x2000); probe.f.close(); break
        except OSError: time.sleep(0.25)
    stop=threading.Event(); state={}
    threading.Thread(target=cc_thread,args=(5761,wps,gear,stop,state),daemon=True).start()

    armed=took=crashed=over=False; hit=[False]*len(wps); peak=0; t0=time.monotonic()
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
        for i,w in enumerate(wps):
            if not hit[i] and dist(lat,lon,w['lat'],w['lon'])<cap: hit[i]=True
        if all(hit): break
    stop.set()
    subprocess.run("podman stop -t 2 fb-aircraft",shell=True,capture_output=True)

    ok_arm=armed; ok_to=took; ok_wp=all(hit)
    abrt = ("CRASH(NaN)" if (crashed and on_nan) else "OVER-CLIMB" if over else None)
    verdict = "PASS" if (ok_arm and ok_to and ok_wp and not abrt) else "FAIL"
    print(f"== {ac} == ARM={'OK' if ok_arm else 'FAIL'} TAKEOFF={'OK' if ok_to else f'FAIL(peak {peak:.0f})'} "
          f"WAYPOINT={sum(hit)}/{len(wps)}{' ABORT:'+abrt if abrt else ''} -> {verdict}")
    sys.exit(0 if verdict=="PASS" else 1)

main()
