#!/usr/bin/env python3
"""PPL(A) THEORY fixtures — iNav-in-the-loop. The theory exam to the missions' practical: each fixture
is (aircraft + environment + initial flight-state + a control/mode script) -> expected iNav/airframe
response, checked as a set of assertions with tolerances. Unlike the pure-libJSBSim fdm_fixtures.cpp,
these fly the REAL vanilla iNav SITL over the JSBSim bridge (the same firmware the missions fly) and
read its telemetry over MSP — so they test what iNav actually does, not just the FDM.

Data-driven: areas live in test/fixtures/*.json (one file per PPL area), each listing which aircraft it
applies to, the setup, the RC/mode script over time, and the asserts. The runner is a thin harness.
Structure target: 3 aircraft x 30 PPL areas x ~10 asserts ~= 900 checks.

  test/ppl_fixtures.py [area-glob] [aircraft]     e.g.  ppl_fixtures.py '06*' c172

Runs in its OWN container (fb-fixture) on an isolated host port, so it never disturbs a running
demo/mission on fb-aircraft. Needs fb-tiles up (ground elevation at spawn).
"""
import os, sys, json, time, math, struct, subprocess, glob
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import MSP

ROOT = Path(__file__).resolve().parents[1]
FIXDIR = ROOT / "test" / "fixtures"
CN = "fb-fixture"; HOST_PORT = 5764                  # isolated from fb-aircraft (5761) so a demo survives
CRUISE = {"c172": 22.0, "sgs233": 18.0, "f16": 55.0}
DEFAULT_HDG = 90.0

# --- MSP telemetry decoders (deci-deg / cm / cm/s on the wire) ---
def u8(p,i): return p[i]
def s16(p,i): return struct.unpack_from("<h", p, i)[0]
def s32(p,i): return struct.unpack_from("<i", p, i)[0]

def sample(m):
    """One telemetry snapshot from iNav over MSP: attitude, altitude/vario, gps, arming/mode flags."""
    d = {}
    a = m.req(108)                                    # MSP_ATTITUDE: roll,pitch (0.1 deg), yaw (deg)
    if a and len(a) >= 6: d["roll"]=s16(a,0)/10.0; d["pitch"]=s16(a,2)/10.0; d["yaw"]=s16(a,4)
    al = m.req(109)                                   # MSP_ALTITUDE: alt (cm, home-rel), vario (cm/s)
    if al and len(al) >= 6: d["alt"]=s32(al,0)/100.0; d["vs"]=s16(al,4)/100.0
    g = m.req(106)                                    # MSP_RAW_GPS: fix,sats,lat,lon,alt(m),speed(cm/s),course
    if g and len(g) >= 16:
        d["fix"]=u8(g,0); d["sats"]=u8(g,1); d["lat"]=s32(g,2)/1e7; d["lon"]=s32(g,6)/1e7
        d["gps_alt"]=s16(g,10); d["gs"]=struct.unpack_from("<H",g,12)[0]/100.0
    st = m.req(0x2000)                                # MSP2_INAV_STATUS: armingFlags @9, modes bitmap later
    if st and len(st) >= 13: d["armflags"]=struct.unpack_from("<I",st,9)[0]; d["armed"]=bool(d["armflags"]&0x4)
    return d

# --- container lifecycle (own name + host port, coexists with fb-aircraft) ---
def spawn(ac, lat, lon, hdg, spawn_alt, spawn_speed, wind_n, wind_e, time_scale):
    subprocess.run(f"podman rm -f {CN}", shell=True, capture_output=True)
    mdir = ROOT/"aircraft"/"models"/ac
    mounts = f" -v {mdir}/profile.env:/app/models/{ac}/profile.env:ro"
    if (mdir/f"{ac}.xml").exists(): mounts += f" -v {mdir}/{ac}.xml:/app/models/{ac}/{ac}.xml:ro"
    ee = mdir/"eeprom.bin"
    if ee.exists(): mounts += f" -v {ee}:/app/models/{ac}/eeprom.bin"
    subprocess.run(
        f"podman run -d --name {CN} --network flightboxnet -p {HOST_PORT}:5761 {mounts} "
        f"-e AIRCRAFT={ac} -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 "
        f"-e WIND_SPEED={math.hypot(wind_n,wind_e):.2f} -e WIND_DIR={(math.degrees(math.atan2(wind_e,wind_n)))%360:.0f} -e TURB=0 "
        f"-e FB_TIME_SCALE={time_scale} -e FLT_LOG_S=3 "
        f"-e ORIGIN_LAT={lat} -e ORIGIN_LON={lon} -e ORIGIN_HDG={hdg} "
        f"-e SPAWN_ALT={spawn_alt:.0f} -e SPAWN_SPEED={spawn_speed:.0f} fb-aircraft"   # image=fb-aircraft, name=fb-fixture
        , shell=True, capture_output=True)
    for _ in range(400):
        try:
            p = MSP("127.0.0.1", HOST_PORT); p.req(0x2000); return p
        except OSError: time.sleep(0.25)
    raise SystemExit(f"{ac}: iNav MSP never came up")

def upload_wps(m, wps):
    for i, w in enumerate(wps):
        b = bytearray(21); b[0]=i+1; b[1]=w.get("action",1)
        b[2:6]=struct.pack("<i",int(w["lat"]*1e7)); b[6:10]=struct.pack("<i",int(w["lon"]*1e7))
        b[10:14]=struct.pack("<i",int(round(w.get("alt_rel",0)*100)))
        b[14:16]=struct.pack("<h",w.get("p1",0)); b[16:18]=struct.pack("<h",w.get("p2",0))
        b[20]=0xa5 if i==len(wps)-1 else 0
        m.send(209, bytes(b))

# --- fixture execution: arm (if asked) -> run the mode/RC script -> collect a metric trace ---
def run_fixture(fx, ac):
    setup = fx.get("setup", {})
    spd = setup.get("speed"); spd = CRUISE[ac] if spd in (None,"cruise") else float(spd)
    lat = setup.get("lat", 52.045); lon = setup.get("lon", 9.385); hdg = setup.get("hdg", DEFAULT_HDG)
    spawn_alt = setup.get("spawn_alt_agl", 300.0)
    wn, we = setup.get("wind_n", 0.0), setup.get("wind_e", 0.0)
    ts = os.environ.get("FB_TIME_SCALE", "3")
    m = spawn(ac, lat, lon, hdg, spawn_alt, spd, wn, we, ts)

    # resolve WP altitudes (AGL -> home-relative): spawn is airborne at spawn_alt, so home alt == spawn_alt AGL;
    # a WP given alt_agl becomes alt_rel = alt_agl - spawn_alt (both above the same field).
    wps = []
    for w in fx.get("waypoints", []):
        wps.append({**w, "alt_rel": w.get("alt_agl", spawn_alt) - spawn_alt})

    dur = fx.get("duration_s", 25)
    script = sorted(fx.get("script", []), key=lambda s: s["t"])
    need_arm = fx.get("arm", True)
    trace = []; t0 = time.monotonic(); cal_done=-1; armed=False; wps_up=False
    while time.monotonic()-t0 < dur:
        ts_now = time.monotonic()-t0
        d = sample(m);
        if d.get("armed"): armed=True
        d["t"] = ts_now; trace.append(d)
        af = d.get("armflags",0)
        rc = [1500,1500,1000,1500,1000,1000,1000,1500,1000,1000]
        # apply the latest script entry whose t <= now (roll/pitch/yaw/thr in RC us, aux flags, mode)
        cur = None
        for s in script:
            if s["t"] <= ts_now: cur = s
        if need_arm and not armed:
            if cal_done<0 and not (af&(1<<9)) and ts_now>0.5: cal_done=ts_now
            if cal_done>=0 and (ts_now-cal_done)%3.0>=1.5: rc[4]=2000; rc[3]=2000   # ARM + YAW_HI bypass
        else:
            if wps and not wps_up: upload_wps(m, wps); wps_up=True
            if cur:
                for k,idx in (("roll",0),("pitch",1),("thr",2),("yaw",3)):
                    if k in cur: rc[idx]=cur[k]
                mode = cur.get("mode","")
                rc[4]=2000                                   # keep armed
                if "ANGLE" in mode: rc[5]=2000
                if "NAV"   in mode: rc[7]=2000
                if "RTH"   in mode: rc[6]=2000
                if cur.get("gear"): rc[8]=2000
                if cur.get("speedbrake"): rc[9]=2000
                if cur.get("link_loss"): m.send(200, struct.pack("<10H",*[900]*10)); time.sleep(0.033); continue
        try: m.send(200, struct.pack("<10H",*rc))
        except OSError: break
        time.sleep(0.05)
    return trace

# --- assertion checks against the steady-state tail of the trace ---
def metric(trace, name, window=8):
    """Mean of `name` over the last `window` seconds of the trace (steady state)."""
    if not trace: return None
    tmax = trace[-1]["t"]; vals=[d[name] for d in trace if d.get("t",0)>=tmax-window and name in d]
    return sum(vals)/len(vals) if vals else None

def check(fx, ac, trace):
    res=[]
    for a in fx["asserts"]:
        aircraft = a.get("aircraft")
        if aircraft and ac not in aircraft: continue
        mname=a["metric"]; win=a.get("window",8)
        if mname=="alt_gain":   v=(metric(trace,"alt",2) or 0)-(trace[0].get("alt",0) if trace else 0)
        elif mname=="hdg_change":                       # heading change over the last `window` s (steady state,
            tmax=trace[-1]["t"] if trace else 0          # excludes the spawn/arm transient)
            ys=[d["yaw"] for d in trace if "yaw" in d and d.get("t",0)>=tmax-win]; v=None
            if len(ys)>=2:
                dv=ys[-1]-ys[0]
                while dv>180:dv-=360
                while dv<-180:dv+=360
                v=dv
        elif mname=="armed":    v=1.0 if any(d.get("armed") for d in trace) else 0.0
        elif mname=="min_airspeed": v=min((d.get("gs",1e9) for d in trace), default=None)
        else: v=metric(trace,mname,win)
        ok=True; why=""
        if v is None: ok=False; why="no data"
        else:
            if "abs_lt" in a and not (abs(v)<a["abs_lt"]): ok=False
            if "gt"    in a and not (v>a["gt"]): ok=False
            if "lt"    in a and not (v<a["lt"]): ok=False
            if "min"   in a and not (v>=a["min"]): ok=False
            if "max"   in a and not (v<=a["max"]): ok=False
        res.append((ok, f"{fx['area']}/{ac}: {a['desc']} [{mname}={v if v is None else round(v,2)}] {why}".rstrip()))
    return res

def main():
    area_glob = sys.argv[1] if len(sys.argv)>1 else "*"
    only_ac = sys.argv[2] if len(sys.argv)>2 else None
    files = sorted(glob.glob(str(FIXDIR/f"{area_glob}.json")))
    if not files: print(f"no fixtures match {area_glob} in {FIXDIR}"); sys.exit(2)
    RUN=FAIL=0; failed=[]
    for f in files:
        fx=json.loads(Path(f).read_text())
        for ac in fx.get("aircraft",["c172"]):
            if only_ac and ac!=only_ac: continue
            print(f"-- {fx['area']} :: {ac} --", flush=True)
            trace=run_fixture(fx, ac)
            for ok,msg in check(fx, ac, trace):
                RUN+=1
                if not ok: FAIL+=1; failed.append(msg); print(f"  [FAIL] {msg}", flush=True)
    subprocess.run(f"podman rm -f {CN}", shell=True, capture_output=True)
    print(f"\n== {RUN} checks, {FAIL} failed -> {'PASS' if FAIL==0 else 'FAIL'} ==")
    sys.exit(0 if FAIL==0 else 1)

if __name__ == "__main__":
    main()
