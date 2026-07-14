#!/usr/bin/env python3
"""FlightBox headless PHYSICS-validation suite.

Connects to the flightbox exactly like the command center (WebSocket on :8080):
reads telemetry, sends control, and asserts that the WHOLE chain
(iNav SITL <-> xp_bridge FDM <-> flightbox <-> telemetry) is PHYSICALLY correct
and realistic — not just that it runs.

The core idea: a real aircraft obeys physical laws, so a real flight model must
too. We fly the autonomous mission, capture a high-rate telemetry trace, and check
hundreds of per-sample physical INVARIANTS over the flight envelope, plus commanded
maneuvers. Each sampled condition is its own test case, so a model that (say) turns
without banking fails hundreds of coordination cases at once.

Usage:  python3 sim/test/eval.py            # containers running on :8080
        HOST=1.2.3.4:8080 python3 ...
        EVAL_FAST=1 python3 ...             # shorter traces (smoke)
Exit code 0 = all PASS.
"""
import socket, struct, base64, os, sys, time, math

HOST = os.environ.get("HOST", "127.0.0.1:8080")
IP, PORT = HOST.split(":")[0], int(HOST.split(":")[1])
FAST = os.environ.get("EVAL_FAST")
TELE = struct.Struct("<IffffffffffffffffffffBBH")  # telem_packet_t (20 floats)
CTRL = struct.Struct("<IffffBBH")                   # ctrl_packet_t
MAG_TELE, MAG_VID, MAG_CTRL = 0x314D4C54, 0x31444956, 0x314C5443
STATES = ["DISARM", "ARMED", "CLIMB", "LOITER", "MANUAL", "RTH"]
G = 9.80665

# ----------------------------------------------------------------------------- IO
class CC:
    """Minimal command-center client: WS to the flightbox."""
    def __init__(self):
        k = base64.b64encode(os.urandom(16)).decode()
        self.s = socket.create_connection((IP, PORT), timeout=4)
        self.s.sendall(("GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
                        "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n" % k).encode())
        r = b""
        while b"\r\n\r\n" not in r:
            r += self.s.recv(1024)
        assert b"101" in r, "WS handshake failed"
        self.s.setblocking(False)
        self.buf = bytearray(); self.telem = None; self.vids = 0; self.n_tele = 0

    def _frames(self):
        try:
            while True:
                d = self.s.recv(65536)
                if not d: break
                self.buf += d
        except BlockingIOError:
            pass
        out = []
        while len(self.buf) >= 2:
            b1 = self.buf[1] & 0x7f; off = 2
            if b1 == 126:
                if len(self.buf) < 4: break
                ln = struct.unpack_from(">H", self.buf, 2)[0]; off = 4
            elif b1 == 127:
                if len(self.buf) < 10: break
                ln = struct.unpack_from(">Q", self.buf, 2)[0]; off = 10
            else:
                ln = b1
            if len(self.buf) < off + ln: break
            out.append(bytes(self.buf[off:off+ln])); del self.buf[:off+ln]
        return out

    def poll(self):
        for f in self._frames():
            if len(f) < 4: continue
            mg = struct.unpack_from("<I", f)[0]
            if mg == MAG_TELE and len(f) == TELE.size:
                self.telem = TELE.unpack(f); self.n_tele += 1
            elif mg == MAG_VID:
                self.vids += 1

    def send(self, roll=0, pitch=0, yaw=0, thr=0.5, arm=1, link=1):
        pl = CTRL.pack(MAG_CTRL, roll, pitch, yaw, thr, arm, link, 0)
        m = os.urandom(4)
        self.s.sendall(bytes([0x82, 0x80 | len(pl)]) + m +
                       bytes(b ^ m[i & 3] for i, b in enumerate(pl)))

    def wait(self, secs, **ctrl):
        out = []; t0 = time.time()
        while time.time() - t0 < secs:
            if ctrl: self.send(**ctrl)
            self.poll()
            if self.telem: out.append(self.telem)
            time.sleep(0.05)
        return out

    def trace(self, secs, hz=12, **ctrl):
        """Timestamped telemetry trace: list of (t, sample-dict)."""
        out = []; t0 = time.time(); nxt = 0.0
        while time.time() - t0 < secs:
            if ctrl: self.send(**ctrl)
            self.poll()
            now = time.time() - t0
            if self.telem and now >= nxt:
                out.append((now, T(self.telem))); nxt += 1.0 / hz
            time.sleep(0.008)
        return out

def T(m):
    return dict(roll=m[1], pitch=m[2], yaw=m[3], alt=m[4], x=m[5], y=m[6], gs=m[7],
                batt=m[8], home=m[9], hbrg=m[10], glide=m[11], vs=m[19],
                airspeed=m[20], state=STATES[m[21] % 6], rssi=m[22])

def wrap180(a):
    while a > 180: a -= 360
    while a < -180: a += 360
    return a

# --------------------------------------------------------------------- reporting
class Suite:
    def __init__(self):
        self.cases = []      # (category, ok)
        self.cat_fail_ex = {}  # category -> a few example failure strings
    def check(self, cat, ok, ex=""):
        self.cases.append((cat, bool(ok)))
        if not ok and ex:
            self.cat_fail_ex.setdefault(cat, [])
            if len(self.cat_fail_ex[cat]) < 3:
                self.cat_fail_ex[cat].append(ex)
    def one(self, name, ok, detail=""):
        self.check(name, ok)
        print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, detail))
    def report(self):
        from collections import OrderedDict
        cats = OrderedDict()
        for cat, ok in self.cases:
            p, n = cats.get(cat, (0, 0))
            cats[cat] = (p + (1 if ok else 0), n + 1)
        print("\n== category breakdown ==")
        multi = [(c, p, n) for c, (p, n) in cats.items() if n > 1]
        for c, p, n in multi:
            tag = "OK " if p == n else "FAIL"
            print("  [%s] %-26s %4d/%-4d" % (tag, c, p, n))
            if p < n and c in self.cat_fail_ex:
                for ex in self.cat_fail_ex[c]:
                    print("        e.g. %s" % ex)
        npass = sum(1 for _, ok in self.cases if ok)
        ntot = len(self.cases)
        print("\n== %d/%d test cases PASS  (%d fail) ==" % (npass, ntot, ntot - npass))
        return 0 if npass == ntot else 1

# --------------------------------------------------------------- physics on trace
def pairs(tr, min_dt=0.04, max_dt=0.6):
    """Consecutive trace samples with a usable dt; yields (dt, a, b)."""
    for i in range(1, len(tr)):
        dt = tr[i][0] - tr[i-1][0]
        if min_dt <= dt <= max_dt:
            yield dt, tr[i-1][1], tr[i][1]

def physics_invariants(S, tr, phase):
    """Per-sample physical invariants over a trace. Each sample = one test case."""
    for dt, a, b in pairs(tr):
        V = max(b["airspeed"], 3.0)
        yaw_rate = wrap180(b["yaw"] - a["yaw"]) / dt          # deg/s
        roll_rate = wrap180(b["roll"] - a["roll"]) / dt       # deg/s
        d_alt = (b["alt"] - a["alt"]) / dt                    # m/s
        d_pos = math.hypot(b["x"] - a["x"], b["y"] - a["y"]) / dt
        roll = b["roll"]
        # "Coordinated turn" theory applies to QUASI-STEADY banked flight. During an
        # active roll input a real wing shows adverse yaw (nose briefly yaws opposite),
        # so only assert coordination when the bank is not rapidly changing.
        steady = abs(roll_rate) < 12

        # (1) COORDINATION: in a STEADY turn, bank and yaw-rate MUST share sign. A real
        #     aircraft cannot fly a sustained left turn while banked right — the crux bug.
        if steady and abs(roll) > 5 and abs(yaw_rate) > 2:
            S.check("coordination(sign)", (roll > 0) == (yaw_rate > 0),
                    "%s roll=%+.1f yawrate=%+.1f/s rr=%+.0f" % (phase, roll, yaw_rate, roll_rate))

        # (2) COORDINATED-TURN RATE: yaw_rate ~ g*tan(bank)/V (rad->deg) in steady flight.
        #     Turbulence/control lag add noise, so the tolerance is generous; the point is
        #     that the magnitude is set by the bank, not invented.
        if steady and abs(roll) > 8 and abs(yaw_rate) > 2:
            exp = math.degrees(G * math.tan(math.radians(roll)) / V)
            S.check("coord-turn-rate", abs(yaw_rate - exp) < max(6.0, 0.6 * abs(exp)),
                    "%s roll=%+.1f yr=%+.1f exp=%+.1f" % (phase, roll, yaw_rate, exp))

        # (3) VERTICAL-SPEED CONSISTENCY: reported vs must equal the actual dAlt/dt.
        vs = 0.5 * (a["vs"] + b["vs"])
        S.check("vs=dAlt/dt", abs(d_alt - vs) < max(1.2, 0.35 * abs(vs)),
                "%s vs=%+.1f dAlt=%+.1f" % (phase, vs, d_alt))

        # (4) GROUNDSPEED CONSISTENCY: |dPos/dt| must equal reported groundspeed.
        gs = 0.5 * (a["gs"] + b["gs"])
        S.check("gs=dPos/dt", abs(d_pos - gs) < max(2.0, 0.35 * gs),
                "%s gs=%.1f dPos=%.1f" % (phase, gs, d_pos))

        # (5) FLIGHT-PATH ANGLE: vs cannot exceed airspeed (|sin(gamma)|<=1) + margin.
        S.check("vs<=airspeed", abs(vs) <= V + 2.0,
                "%s vs=%+.1f V=%.1f" % (phase, vs, V))

        # (6) ENVELOPE BOUNDS: no blow-up, physically plausible attitudes/speeds.
        ok = (abs(b["roll"]) < 75 and abs(b["pitch"]) < 55 and 0 <= b["airspeed"] < 45
              and b["alt"] > -5 and all(map(math.isfinite,
                  (b["roll"], b["pitch"], b["yaw"], b["airspeed"], b["alt"]))))
        S.check("envelope-bounds", ok,
                "%s roll=%.0f pitch=%.0f V=%.1f alt=%.0f" %
                (phase, b["roll"], b["pitch"], b["airspeed"], b["alt"]))

        # (7) ATTITUDE CONTINUITY: no teleport between samples (rate-limited).
        S.check("attitude-continuous",
                abs(wrap180(b["roll"] - a["roll"])) / dt < 400 and
                abs(wrap180(b["pitch"] - a["pitch"])) / dt < 300,
                "%s dRoll=%.0f/s" % (phase, wrap180(b["roll"] - a["roll"]) / dt))

    # per-trace derived checks
    for _, s in tr:
        # (8) HOME DISTANCE = |position| (telemetry self-consistency)
        S.check("home=|pos|", abs(s["home"] - math.hypot(s["x"], s["y"])) < max(8.0, 0.06 * s["home"]),
                "%s home=%.0f |pos|=%.0f" % (phase, s["home"], math.hypot(s["x"], s["y"])))
        # (9) AIRSPEED / GROUNDSPEED both physical, wind-bounded difference
        S.check("gs-vs-airspeed", abs(s["gs"] - s["airspeed"]) < 15.0,
                "%s gs=%.1f as=%.1f" % (phase, s["gs"], s["airspeed"]))

# ---------------------------------------------------------------------------- run
def main():
    S = Suite()
    print("== FlightBox PHYSICS validation (%s) ==" % HOST)

    # 0) flightbox HTTP / WASM subsystem -------------------------------------
    def http_get(path):
        c = socket.create_connection((IP, PORT), timeout=4)
        c.sendall(("GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n" % path).encode())
        d = b""
        try:
            while True:
                b = c.recv(65536)
                if not b: break
                d += b
        except Exception: pass
        c.close(); return d
    idx = http_get("/"); wasm = http_get("/cc.wasm"); cfg = http_get("/config.js")
    S.one("HTTP serves index", b"200" in idx.split(b"\r\n", 1)[0] and b"text/html" in idx)
    S.one("HTTP serves WASM (mime)", b"200" in wasm.split(b"\r\n", 1)[0] and b"application/wasm" in wasm)
    S.one("config.js origin", b"ORIGIN" in cfg or b"olat" in cfg.lower() or b"200" in cfg.split(b"\r\n",1)[0])

    cc = CC()

    # 1) telemetry + video pipeline ------------------------------------------
    cc.wait(3)
    S.one("telemetry received", cc.telem is not None)
    S.one("video stream", cc.vids > 10, "frames=%d" % cc.vids)
    if cc.telem is None:
        print("no telemetry -> abort"); return S.report()
    # telemetry rate ~ high (game-engine sampling)
    n0 = cc.n_tele; cc.wait(2.0); rate = (cc.n_tele - n0) / 2.0
    S.one("telemetry rate > 30 Hz", rate > 30, "%.0f Hz" % rate)

    # 2) autonomous launch ----------------------------------------------------
    armed_at = flying_at = None; t0 = time.time()
    while time.time() - t0 < 30 and not (armed_at and flying_at):
        cc.poll()
        if cc.telem:
            m = T(cc.telem)
            if armed_at is None and m["rssi"] > 0: armed_at = time.time() - t0
            if flying_at is None and m["gs"] > 5:  flying_at = time.time() - t0
        time.sleep(0.1)
    S.one("auto-arm (senderless)", armed_at is not None, "t=%.1fs" % (armed_at or -1))
    S.one("airborne", flying_at is not None, "t=%.1fs" % (flying_at or -1))

    # wait until established above 80 m so we're clearly flying
    t0 = time.time()
    while time.time() - t0 < 60:
        cc.poll()
        if cc.telem and T(cc.telem)["alt"] > 80: break
        time.sleep(0.1)

    # 3) PHYSICS on the autonomous CLIMB trace -------------------------------
    print("\n-- capturing climb trace --")
    tr_climb = cc.trace(10 if FAST else 22)
    physics_invariants(S, tr_climb, "climb")

    # 4) COMMANDED MANEUVERS: bank->turn causality (the coupling test) --------
    #    Hold a bank command; heading must rotate in the SAME direction as the bank,
    #    and reverse when the bank reverses. Sweep both directions and magnitudes.
    print("-- commanded bank/turn coupling --")
    for label, rollcmd in [("right-hard", 0.7), ("left-hard", -0.7),
                           ("right-med", 0.4), ("left-med", -0.4)]:
        tr = cc.trace(6, roll=rollcmd, thr=0.6)
        if len(tr) > 4:
            mid = [s for _, s in tr[len(tr)//3:]]
            mean_roll = sum(s["roll"] for s in mid) / len(mid)
            yaw_rate = wrap180(tr[-1][1]["yaw"] - tr[len(tr)//3][1]["yaw"]) / \
                       max(0.1, tr[-1][0] - tr[len(tr)//3][0])
            # bank achieved in commanded direction
            S.check("cmd-bank-direction", (mean_roll > 3) == (rollcmd > 0) or abs(mean_roll) > 3
                    and (mean_roll > 0) == (rollcmd > 0),
                    "%s cmd=%+.1f roll=%+.1f" % (label, rollcmd, mean_roll))
            # turn follows bank sign (coordination under command)
            if abs(mean_roll) > 3:
                S.check("cmd-turn-follows-bank", (mean_roll > 0) == (yaw_rate > 0),
                        "%s roll=%+.1f yawrate=%+.1f" % (label, mean_roll, yaw_rate))
        physics_invariants(S, tr, "cmd:" + label)

    # 5) PITCH authority: nose up command raises pitch / climb ----------------
    print("-- commanded pitch --")
    base = T(cc.telem)
    tu = cc.trace(4, pitch=0.6, thr=0.6); td = cc.trace(4, pitch=-0.6, thr=0.6)
    if tu and td:
        pu = sum(s["pitch"] for _, s in tu) / len(tu)
        pd = sum(s["pitch"] for _, s in td) / len(td)
        S.check("cmd-pitch-authority", (pu - pd) > 4, "up=%.1f dn=%.1f" % (pu, pd))

    # 6) THROTTLE -> speed: more throttle, more airspeed (level-ish) ----------
    print("-- throttle/speed --")
    thi = cc.trace(6, thr=0.95); tlo = cc.trace(6, thr=0.15)
    if thi and tlo:
        vhi = sum(s["airspeed"] for _, s in thi[len(thi)//2:]) / max(1, len(thi) - len(thi)//2)
        vlo = sum(s["airspeed"] for _, s in tlo[len(tlo)//2:]) / max(1, len(tlo) - len(tlo)//2)
        S.check("throttle-speed-monotonic", vhi > vlo + 1.0, "vhi=%.1f vlo=%.1f" % (vhi, vlo))

    # 7) Hand back to autopilot, capture LOITER, check geometry + physics -----
    print("-- capturing loiter trace --")
    cc.wait(6)  # let the autopilot re-capture
    tr_loi = cc.trace(14 if FAST else 40)
    physics_invariants(S, tr_loi, "loiter")
    loi = [s for _, s in tr_loi if s["state"] in ("LOITER", "RTH")]
    if len(loi) > 10:
        homes = [s["home"] for s in loi]
        mean_r = sum(homes) / len(homes)
        S.one("loiter is a WIDE circle (r>500m)", mean_r > 500, "mean r=%.0fm" % mean_r)
        S.one("loiter bounded (r<1600m)", max(homes) < 1600, "max r=%.0fm" % max(homes))
        # circularity: yaw should sweep continuously one way (a real orbit)
        yaws = [s["yaw"] for s in loi]
        dyaws = [wrap180(yaws[i] - yaws[i-1]) for i in range(1, len(yaws))]
        same = sum(1 for d in dyaws if d > 0); tot = len(dyaws)
        frac = max(same, tot - same) / max(1, tot)
        S.one("loiter turns one direction (>80%)", frac > 0.80, "%.0f%% consistent" % (frac*100))

    # 8) RC-loss failsafe: autonomous RTH loiter over home --------------------
    print("-- RC-loss failsafe --")
    fs = [T(x) for x in cc.wait(28, arm=1, link=0)]
    if fs:
        S.one("RC-loss -> RTH state", any(x["state"] == "RTH" for x in fs))
        S.one("RC-loss -> link shows 0", any(x["rssi"] == 0 for x in fs))
        S.one("failsafe bounded", max(x["home"] for x in fs) < 1600,
              "max=%.0fm" % max(x["home"] for x in fs))
        S.one("failsafe airborne", min(x["alt"] for x in fs) > 40,
              "min=%.0fm" % min(x["alt"] for x in fs))

    return S.report()

if __name__ == "__main__":
    sys.exit(main())
