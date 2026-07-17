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
TELEM_DT  = 0.01   # s     xp_bridge.c main loop: `const double dt=0.01`, one telem packet and
                   #       one seq increment per tick. seq delta x this = the TRUE sample spacing.

# telem.alt is ASL now (commit 7bbc362): the plane reports GPS/geodetic height and AGL is no longer
# on the wire -- it stays FDM-internal (S.agl). The thresholds below that used to mean "height above
# GROUND" therefore have to add the ground datum back in. GROUND_ASL is the origin's ground elevation
# ASL: default 71.0 m matches xp_bridge.c's HOME_ELEV seed AND the fb-tiles DEM at Hameln (measured
# 70.9 m in the container log). ASSUMPTION, valid because the physics traces orbit the HOME region:
# the ground under the loitering aircraft barely varies from this value there. Over strongly varied
# terrain a real per-position ground would be needed (eval.py could reconstruct AGL = alt - /elev the
# way the base station does) -- deliberately not built; not needed for home traces. Env-overridable
# for a foreign origin.
GROUND_ASL = float(os.environ.get("HOME_ELEV", 71.0))   # m, origin ground elevation ASL

# --- steadiness gate for the coordinated-turn checks (see coord_scan) -----------
DWELL_W   = 0.6    # s     attitude must have been quiet for this whole trailing window
DWELL_RR  = 12.0   # deg/s "quiet" threshold for the ROLL rate inside that window
DWELL_QQ  = 15.0   # deg/s "quiet" threshold for the PITCH rate inside that window
AVG_W     = 1.5    # s     averaging window for the windowed checks
TURN_W    = 2.5    # s     dwell the WINDOWED checks need: the bank settles well before the
                   #       turn does, so "bank steady" does not yet mean "steady turn"
EXP_HI    = 5.0    # deg/s |expected yaw rate| above which per-sample signs must agree
EXP_LO    = 2.0    # deg/s below this the turn is slower than the turbulence — unprovable

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
                airspeed=m[20], state=STATES[m[21] % 6], rssi=m[22], seq=m[23])

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
        # Every MULTI-sample category, plus every FAILING one whatever its size. The n>1
        # filter alone silently swallowed single-case categories: cmd-pitch-authority and
        # throttle-speed-monotonic are checked exactly once each, so when one of them went
        # red the run printed "1 fail" and a breakdown in which everything was OK. A red you
        # cannot see is not a test — you cannot fix what the report will not name.
        multi = [(c, p, n) for c, (p, n) in cats.items() if n > 1 or p < n]
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
def derive(tr, min_dt=0.04, max_dt=0.6):
    """Consecutive trace samples with a usable dt -> list of derived-quantity dicts.

    A LIST, not a generator: the steadiness gate below needs to look BACKWARDS over a
    time window, which a per-pair loop cannot do.

    dt COMES FROM `seq`, NEVER FROM ARRIVAL TIME. Telemetry is emitted every 10 ms tick and
    stamped with a sequence number; it then crosses UDP and a WebSocket and arrives BATCHED,
    several packets in one instant. Wall-clock spacing therefore measures when we happened to
    poll, not when the aircraft was in that state, and dividing a real position change by a
    fictitious dt inflates the derivative. That is not hypothetical: this suite reported
    `gs=22.8 dPos=32.9` and `gs=21.4 dPos=31.0` — a consistent ~1.44x, on an aircraft whose
    own reported groundspeed was perfectly sane. protocol.h carries a _Static_assert about it
    ("seq is last -> receivers space samples by it, never by arrival time") and xp_bridge.c
    spells out the failure mode at the send site; this consumer simply ignored both, and T()
    dropped the field entirely. seq delta x 10 ms is the true spacing.
    """
    out = []; t_acc = 0.0
    for i in range(1, len(tr)):
        a, b = tr[i-1][1], tr[i][1]
        dseq = (b["seq"] - a["seq"]) & 0xFFFF   # uint16, wraps
        if dseq == 0: continue                  # same packet polled twice: zero new information
        dt = dseq * TELEM_DT
        t_acc += dt                             # advance real time even across gaps we skip
        if not (min_dt <= dt <= max_dt): continue
        V = max(b["airspeed"], 3.0)
        phi, th = math.radians(b["roll"]), math.radians(b["pitch"])
        d_psi = wrap180(b["yaw"] - a["yaw"]) / dt              # EULER heading rate, deg/s
        d_th = (b["pitch"] - a["pitch"]) / dt                  # EULER pitch rate, deg/s
        # BODY yaw rate r, from the Euler kinematics
        #     dtheta = cos(phi) q - sin(phi) r ;  dpsi = (sin(phi) q + cos(phi) r)/cos(theta)
        # inverted (the 2x2 is a rotation, so its inverse is its transpose):
        #     r = -sin(phi) dtheta + cos(phi) dpsi cos(theta)
        # This matters: the HEADING rate is not the turn. A banked aircraft that lowers its
        # nose swings its heading through the q*sin(phi) term with no yaw moment whatever —
        # and at 21 deg of bank pitching down 24 deg/s that term (-8.8 deg/s) BURIES the real
        # turn (+3.5 deg/s) and inverts the heading rate's sign. Checking the heading rate
        # then reports a perfectly coordinated aircraft as turning the wrong way. Measured on
        # a real trace: exactly that produced a red sample at TURB=0, rescued by r.
        r_body = -math.sin(phi) * d_th + math.cos(phi) * d_psi * math.cos(th)
        out.append(dict(
            t=t_acc, dt=dt, a=a, b=b, V=V, roll=b["roll"],
            yaw_rate=d_psi,                                    # kept for reporting only
            r_body=r_body,
            roll_rate=wrap180(b["roll"] - a["roll"]) / dt,     # deg/s
            pitch_rate=d_th,                                   # deg/s
            d_alt=(b["alt"] - a["alt"]) / dt,                  # m/s
            d_pos=math.hypot(b["x"] - a["x"], b["y"] - a["y"]) / dt,
            # In that same steady turn the BODY yaw rate is r = g*sin(phi)/V — the heading-rate
            # form g*tan(phi)/V would be right only for dpsi. They differ by exactly cos(phi):
            # 7% at 21 deg, 29% at 45 deg. Pairing r with tan() would have quietly built that
            # error into the tolerance. (xp_bridge.c's yaw damping uses g*sin(phi)/V too.)
            exp=math.degrees(G * math.sin(phi) / V)))
    return out

def dwell_ok(seq, i, W=DWELL_W, rrmax=DWELL_RR, qqmax=DWELL_QQ):
    """Has the aircraft been NOT MANOEUVRING for the whole trailing W seconds?

    Two separate lessons are baked in here.

    (a) A DWELL, not a derivative. The old gate was the INSTANTANEOUS |roll_rate| < 12
        deg/s, which is structurally wrong: roll_rate passes through zero at every turning
        point of a roll oscillation — including the apex of a 141 deg/s roll slam, where
        the aircraft is at its least steady. A momentary derivative cannot express
        "quasi-steady"; only a trailing window can.

    (b) PITCH counts too. g*sin(phi)/V describes a steady, level-ish turn: it assumes the
        aircraft is not also pitching. Gating on roll alone leaves the pitch transient in —
        measured, an aircraft 0.4 s into a CLIMB->MANUAL handover was holding a near-constant
        bank (roll_rate ~ 0, so the roll gate happily admitted it) while pitching down at
        24 deg/s, and its body yaw rate was 35% of the steady-turn value. The bank being
        quiet does not make the flight quiet.
    """
    t = seq[i]["t"]; j = i; covered = False
    while j >= 0 and t - seq[j]["t"] <= W:
        if abs(seq[j]["roll_rate"]) >= rrmax: return False
        if abs(seq[j]["pitch_rate"]) >= qqmax: return False
        if t - seq[j]["t"] >= 0.8 * W: covered = True
        j -= 1
    return covered      # need real history: the first samples of a trace prove nothing

def win_mean(seq, i, W=AVG_W):
    """Trailing-window means of (roll, r_body, V), or None if the window isn't covered."""
    t = seq[i]["t"]; j = i; rs = []; ys = []; vs = []
    while j >= 0 and t - seq[j]["t"] <= W:
        rs.append(seq[j]["roll"]); ys.append(seq[j]["r_body"]); vs.append(seq[j]["V"]); j -= 1
    if t - seq[j+1]["t"] < 0.8 * W: return None
    return sum(rs)/len(rs), sum(ys)/len(ys), sum(vs)/len(vs)

def coord_scan(seq, flip_below=None):
    """The coordinated-turn checks. Yields (category, ok, detail) per gated sample.

    `flip_below` is FAULT INJECTION: negate the yaw rate wherever |roll| < that many
    degrees, i.e. make the model turn the WRONG WAY — the exact shape of the worst bug
    this project ever had. The self-test drives this function with a fault so that the
    gates are proven able to fail. Same code path as the real checks, deliberately:
    a self-test against a reimplementation proves nothing about the implementation.
    """
    for i in range(len(seq)):
        s = seq[i]
        roll, exp = s["roll"], s["exp"]
        r_body = s["r_body"]
        if flip_below is not None and abs(roll) < flip_below: r_body = -r_body
        # (1a) COORDINATION, per sample: in a STEADY turn, bank and BODY YAW RATE must share
        #      sign. A real aircraft cannot fly a sustained left turn while banked right.
        #      Gated on the EXPECTED yaw rate rather than on bank: it is the signal being
        #      tested, and it scales with airspeed the way the bank alone does not. The sign
        #      survives per-sample where the magnitude does not — a gross wrong-way turn is
        #      visible in one sample, so this one stays instantaneous.
        if dwell_ok(seq, i) and abs(exp) > EXP_HI and abs(r_body) > 2:
            yield ("coordination(sign)", (roll > 0) == (r_body > 0),
                   "roll=%+.1f r_body=%+.1f/s (headrate=%+.1f) rr=%+.0f"
                   % (roll, r_body, s["yaw_rate"], s["roll_rate"]))

        # --- the WINDOWED checks. Two window lessons, both measured, both learned the hard way.
        #     (i)  The dwell must cover the WHOLE averaging window, not just the trailing
        #          DWELL_W: a mean is only as steady as its oldest sample. With a 0.6 s dwell
        #          under a 1.5 s mean the window reached 0.9 s back into a roll transient the
        #          gate had rejected — 2 false positives at TURB=1.0; covering it: 0.
        #     (ii) TURN_W > AVG_W because the BANK SETTLES BEFORE THE TURN DOES. Holding
        #          aileron, roll_rate and pitch_rate go quiet while the yaw rate is still
        #          building, so a roll+pitch dwell happily certifies a turn that has not
        #          converged yet. Measured on the Skywalker-X8's held 6 s turn: dwell 1.5 s
        #          admits samples down to 46% of the coordinated rate and reds 8 of 86; dwell
        #          2.0 s -> 0 red but the worst sample sits at 0.61 against a 0.60 floor, which
        #          is no margin at all; 2.5 s -> 0 red, worst 0.65, at a cost of 24 samples.
        if not dwell_ok(seq, i, W=TURN_W): continue
        w = win_mean(seq, i)
        if not w: continue
        mr, my, mv = w
        if flip_below is not None and abs(mr) < flip_below: my = -my
        mexp = math.degrees(G * math.sin(math.radians(mr)) / mv)   # body r, not heading rate

        # (2) COORDINATED-TURN RATE: r ~ g*sin(bank)/V. WINDOWED, because per sample this
        #     measures turbulence, not the model: the instantaneous error tail ran to 7.2 deg/s
        #     at TURB=1.0 while the ratio's median sat at ~1 — i.e. the FDM is unbiased and the
        #     spread is the air. The old per-sample form had to tolerate 6.0 deg/s (a 60% error
        #     at 21 deg of bank) to survive that noise, which is most of the way to no check at
        #     all. Averaging is zero-mean on the turbulence and leaves a systematic error fully
        #     visible, so the windowed check asserts the physics ~1.5x harder (40% at 21 deg).
        #     Measured on real traces: max |err| 1.2 deg/s (TURB=0) / 2.6 (TURB=1.0) against
        #     this 3.5 floor, while an injected 1.5x yaw-rate gain error still reddens 41-49%
        #     of gated samples. The margin is deliberate — a suite that must stay green across
        #     4 airframes x 2 weathers cannot be calibrated to the edge of one trace.
        if abs(mr) > 8 and abs(my) > 2:
            yield ("coord-turn-rate", abs(my - mexp) < max(3.5, 0.40 * abs(mexp)),
                   "mroll=%+.1f m_r=%+.1f mexp=%+.1f" % (mr, my, mexp))

        # (1b) COORDINATION, windowed: below ~9 deg of bank the expected yaw rate sinks into
        #      the turbulence and no single sample can prove anything — but a 1.5 s MEAN can.
        #      Without this the sign check only ever fires in the mission's steep band and a
        #      wrong-way turn at low bank goes completely unnoticed (measured: 0 detections).
        if EXP_LO < abs(mexp) <= EXP_HI and abs(my) > 1.5:
            yield ("coordination(sign,avg)", (mr > 0) == (my > 0),
                   "mroll=%+.1f m_r=%+.1f/s" % (mr, my))

def physics_invariants(S, tr, phase):
    """Per-sample physical invariants over a trace. Each sample = one test case."""
    seq = derive(tr)

    # (1)+(2) COORDINATION and COORDINATED-TURN RATE. "Coordinated turn" theory applies to
    #         QUASI-STEADY banked flight: during an active roll input a real wing shows
    #         adverse yaw (nose briefly yaws opposite), and flagging that is a false
    #         positive. coord_scan owns the gating — and the fault-injection self-test
    #         drives that same function to prove the gates can still go red.
    for cat, ok, detail in coord_scan(seq):
        S.check(cat, ok, "%s %s" % (phase, detail))

    for s in seq:
        a, b, V = s["a"], s["b"], s["V"]
        d_alt, d_pos = s["d_alt"], s["d_pos"]

        # (3) VERTICAL-SPEED CONSISTENCY: reported vs must equal the actual dAlt/dt. With alt=ASL
        #     this is now structurally EXACT -- alt (S.elev) and vs (S.vy) are the same integration
        #     (xp_bridge.c: S.elev+=climb*dt, S.vy=climb), so d(ASL)/dt == vs but for the trapezoidal
        #     window error; under AGL the two diverged over sloping terrain by d(ground)/dt. Measured
        #     on a home trace: |d_alt-vs| max 0.06 m/s over 742 samples (p99 0.06), ~19x inside this
        #     floor. The tolerance is deliberately NOT tightened to that: this invariant is a
        #     DIVERGENCE guard (a wrong unit, sign or integrand shows up as a gross mismatch, not
        #     0.1 m/s), and 1.2/0.35 is the cross-airframe/weather margin -- one trace cannot
        #     recalibrate what the 4x2 matrix must survive. See coord-turn-rate for the same lesson.
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

        # (6) ENVELOPE BOUNDS: no blow-up, physically plausible attitudes/speeds. The altitude term
        #     is a COARSE ASL FLOOR now, not the old AGL ground-penetration check: with alt=ASL and
        #     AGL gone from the wire, "5 m below ground" is no longer reconstructable per sample, so
        #     this can only catch a gross sink / integration blow-up (a NaN is caught by isfinite).
        #     The aircraft never legitimately sits below the home ground (~71 m ASL); GROUND_ASL-30
        #     clears DEM/seed slack and terrain a little below home while still biting on a blow-up.
        ok = (abs(b["roll"]) < 75 and abs(b["pitch"]) < 55 and 0 <= b["airspeed"] < 45
              and b["alt"] > GROUND_ASL - 30 and all(map(math.isfinite,
                  (b["roll"], b["pitch"], b["yaw"], b["airspeed"], b["alt"]))))
        S.check("envelope-bounds", ok,
                "%s roll=%.0f pitch=%.0f V=%.1f alt=%.0f" %
                (phase, b["roll"], b["pitch"], b["airspeed"], b["alt"]))

        # (7) ATTITUDE CONTINUITY: no teleport between samples (rate-limited).
        S.check("attitude-continuous",
                abs(s["roll_rate"]) < 400 and
                abs(wrap180(b["pitch"] - a["pitch"])) / s["dt"] < 300,
                "%s dRoll=%.0f/s" % (phase, s["roll_rate"]))

    # per-trace derived checks
    for _, s in tr:
        # (8) HOME DISTANCE = |position| (telemetry self-consistency)
        S.check("home=|pos|", abs(s["home"] - math.hypot(s["x"], s["y"])) < max(8.0, 0.06 * s["home"]),
                "%s home=%.0f |pos|=%.0f" % (phase, s["home"], math.hypot(s["x"], s["y"])))
        # (9) AIRSPEED / GROUNDSPEED both physical, wind-bounded difference
        S.check("gs-vs-airspeed", abs(s["gs"] - s["airspeed"]) < 15.0,
                "%s gs=%.1f as=%.1f" % (phase, s["gs"], s["airspeed"]))

def gate_selftest(S, traces):
    """A test OF THE TEST: inject a wrong-way turn into the REAL traces and require the
    coordination gates to go red.

    A green check proves nothing unless it is also capable of failing. Every tightening of
    a gate trades false positives for blindness, and nothing in a normal run tells you when
    you have crossed into decoration: an over-tightened filter reports the same "PASS" as a
    working one. So we re-fly the captured trace with the fault the gate exists to catch —
    the sign flip that WAS the worst bug in this project's history — and assert detection.
    Costs no flight time: it re-uses the traces already captured.

    The ceilings matter. A per-sample sign check only fires where the expected yaw rate
    stands out of the turbulence (~9 deg of bank up); measured, a fault confined below
    15 deg was caught 0 times by the per-sample gate alone. The windowed low-bank check
    exists precisely to close that hole, and this is what proves it stays closed.
    """
    # Each ceiling names the category that is SUPPOSED to catch it, and only that category is
    # scored. A first cut pooled all coordination samples and demanded 50% red, which quietly
    # made the threshold depend on the sample MIX rather than on detection: give the aircraft
    # time to settle into steep turns and the windowed check's share of the pool drops, so the
    # pooled rate fell to 37.5% and the self-test reddened while detection was in fact perfect.
    # A test whose verdict moves with the mission profile measures the mission, not the gate.
    for ceiling, cat_want, min_rate in ((90.0, "coordination(sign)",     80.0),
                                        (90.0, "coordination(sign,avg)", 80.0),
                                        # Below 15 deg the per-sample check is BLIND by design
                                        # (measured: 0/106) — the windowed one is the whole
                                        # defence, so it alone is scored here.
                                        (15.0, "coordination(sign,avg)", 80.0)):
        gated = fails = 0
        for tr in traces:
            for cat, ok, _ in coord_scan(derive(tr), flip_below=ceiling):
                if cat != cat_want: continue
                gated += 1
                if not ok: fails += 1
        rate = (100.0 * fails / gated) if gated else 0.0
        S.one("fault-injection: wrong-way turn below %2.0f deg caught by %s"
              % (ceiling, cat_want),
              gated > 20 and rate >= min_rate,
              "%d/%d gated samples red (%.1f%%)" % (fails, gated, rate))

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

    # wait until 80 m ABOVE GROUND so we're clearly flying, not still on climb-out. alt is ASL now:
    # a bare alt>80 would be ~9 m AGL at Hameln and start the climb trace almost on the deck.
    t0 = time.time()
    while time.time() - t0 < 60:
        cc.poll()
        if cc.telem and T(cc.telem)["alt"] > GROUND_ASL + 80: break
        time.sleep(0.1)

    # 3) PHYSICS on the autonomous CLIMB trace -------------------------------
    print("\n-- capturing climb trace --")
    traces = []          # kept for the fault-injection self-test at the end
    tr_climb = cc.trace(10 if FAST else 22)
    traces.append(tr_climb)
    physics_invariants(S, tr_climb, "climb")

    # 4) COMMANDED MANEUVERS: bank->turn causality (the coupling test) --------
    #    Hold a bank command; heading must rotate in the SAME direction as the bank,
    #    and reverse when the bank reverses. Sweep both directions and magnitudes.
    print("-- commanded bank/turn coupling --")
    for label, rollcmd in [("right-hard", 0.7), ("left-hard", -0.7),
                           ("right-med", 0.4), ("left-med", -0.4)]:
        # ESTABLISH the turn before measuring it. These four used to run back-to-back, so
        # "right-hard" began the instant the climb trace ended and "left-hard" tore straight
        # out of the opposite turn — the aircraft was never in a steady turn at all, and
        # coordinated-turn theory describes nothing else. It shows up airframe-dependently:
        # the Skywalker-X8 is heavy and rudderless, its yaw settles long after its bank does,
        # and measured in the suite's back-to-back sequence its turn reached only 0.51 of the
        # coordinated rate — against 0.67 for the identical command once allowed to settle.
        # Asserting steady-state physics on a transient is a bug in the measurement, not a
        # finding about the model. 3 s of the same command first; then measure.
        cc.wait(3, roll=rollcmd, thr=0.6)
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
        traces.append(tr)
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
    #    The yaw=0.2 is LOAD-BEARING, not decoration. xp_bridge.c decides "the operator has
    #    the sticks" with  stick = (|roll|>0.15 || |pitch|>0.15 || |yaw|>0.15)  — THROTTLE IS
    #    NOT IN THAT TEST. So a throttle-only command never leaves the autonomous mission, and
    #    that autopilot holds airspeed at CRUISE_V *using the throttle*. This check therefore
    #    used to measure the speed controller rejecting a disturbance it never even saw:
    #    measured, thr=0.95 -> 18.2 m/s and thr=0.15 -> 18.3 m/s, both in LOITER, i.e. the
    #    commanded throttle was discarded and the verdict was decided by which way the noise
    #    fell against the 1.0 m/s threshold. It passed on three airframes by luck and failed on
    #    the AR-Wing. A small yaw stick hands control over without touching the speed axis
    #    (pitch or roll would confound it), and the plant then answers properly: 28.4 vs 13.6.
    print("-- throttle/speed --")
    thi = cc.trace(8, thr=0.95, yaw=0.2); tlo = cc.trace(8, thr=0.15, yaw=0.2)
    if thi and tlo:
        vhi = sum(s["airspeed"] for _, s in thi[len(thi)//2:]) / max(1, len(thi) - len(thi)//2)
        vlo = sum(s["airspeed"] for _, s in tlo[len(tlo)//2:]) / max(1, len(tlo) - len(tlo)//2)
        shi = set(s["state"] for _, s in thi[len(thi)//2:])
        # Prove the premise instead of assuming it: if we are not in MANUAL, the number above
        # is the autopilot's cruise speed and says nothing about throttle authority.
        S.check("throttle-test-in-manual", shi == {"MANUAL"}, "states=%s" % sorted(shi))
        S.check("throttle-speed-monotonic", vhi > vlo + 1.0, "vhi=%.1f vlo=%.1f" % (vhi, vlo))

    # RELEASE THE STICKS — this is mandatory, not tidiness. xp_bridge.c holds the last control
    # packet in file-scope cr/cp/cy (line ~557, outside the loop) and re-evaluates
    #   stick = (|cr|>0.15 || |cp|>0.15 || |cy|>0.15);  if(stick&&link_up) last_input=ts;
    # EVERY tick from those retained values. So merely ceasing to send does not hand control
    # back: the last non-neutral stick keeps re-arming last_input and MANUAL latches forever.
    # The old throttle test (sticks at zero) was accidentally the thing that released it after
    # the bank sweep; giving it a real yaw stick removed that side effect and the aircraft sat
    # in MANUAL at 15% throttle for 150 s and flew into the ground. An explicit neutral hand-back
    # is what the old code got by luck.
    cc.wait(3, roll=0, pitch=0, yaw=0, thr=0.6)

    # 7) Hand back to autopilot, capture LOITER, check geometry + physics -----
    print("-- capturing loiter trace --")
    # WAIT FOR THE STATE, don't guess a duration. This was a fixed 6 s, which was only ever
    # enough because the throttle test above did nothing: the aircraft never actually left the
    # orbit, so 6 s of "re-capture" re-captured an aircraft that had never gone anywhere. Once
    # the throttle command genuinely reached the plant, the manual segment left it low, slow and
    # far out, 6 s was nowhere near enough, and the loiter trace was captured mid-climb-home.
    # `state` then never reached LOITER, `len(loi) > 10` was False, and the three loiter-geometry
    # checks SILENTLY VANISHED — the run printed one fewer failure and looked better for it.
    # Skipping a check must never be the quiet outcome, so the recovery is now asserted.
    t0 = time.time(); st = "?"
    while time.time() - t0 < 150:
        cc.poll()
        if cc.telem:
            st = T(cc.telem)["state"]
            if st == "LOITER": break
        time.sleep(0.2)
    S.one("autopilot re-establishes LOITER after manual", st == "LOITER",
          "t=%.0fs state=%s" % (time.time() - t0, st))
    cc.wait(5)   # settle on the orbit before measuring its geometry
    tr_loi = cc.trace(14 if FAST else 40)
    traces.append(tr_loi)
    physics_invariants(S, tr_loi, "loiter")
    loi = [s for _, s in tr_loi if s["state"] in ("LOITER", "RTH")]
    S.one("loiter trace is actually in LOITER/RTH", len(loi) > 10, "n=%d" % len(loi))
    if len(loi) > 10:
        homes = [s["home"] for s in loi]
        mean_r = sum(homes) / len(homes)
        S.one("loiter is a WIDE circle (r>500m)", mean_r > 500, "mean r=%.0fm" % mean_r)
        S.one("loiter bounded (r<1600m)", max(homes) < 1600, "max r=%.0fm" % max(homes))
        # A clean circle keeps the home distance in a TIGHT band around its mean. (Checking
        # per-sample yaw-rate sign is misleading here: a 1000 m orbit needs only ~1 deg of
        # bank, so its yaw-rate is tiny and turbulence flips individual sample deltas even
        # though the orbit itself is perfectly steady.)
        var = sum((h - mean_r) ** 2 for h in homes) / len(homes)
        std = var ** 0.5
        S.one("loiter is a tight circle (std<15% of r)", std < 0.15 * mean_r,
              "std=%.0fm (%.0f%% of r)" % (std, 100 * std / mean_r))

    # 8) RC-loss failsafe: autonomous RTH loiter over home --------------------
    print("-- RC-loss failsafe --")
    fs = [T(x) for x in cc.wait(28, arm=1, link=0)]
    if fs:
        S.one("RC-loss -> RTH state", any(x["state"] == "RTH" for x in fs))
        S.one("RC-loss -> link shows 0", any(x["rssi"] == 0 for x in fs))
        S.one("failsafe bounded", max(x["home"] for x in fs) < 1600,
              "max=%.0fm" % max(x["home"] for x in fs))
        # airborne = still >= 40 m ABOVE GROUND (alt is ASL); a bare >40 would be true underground.
        S.one("failsafe airborne", min(x["alt"] for x in fs) > GROUND_ASL + 40,
              "min=%.0fm ASL" % min(x["alt"] for x in fs))

    # 9) Does the coordination gate still WORK? Inject the fault and demand a red. -----
    print("-- gate self-test (fault injection) --")
    gate_selftest(S, traces)

    return S.report()

if __name__ == "__main__":
    sys.exit(main())
