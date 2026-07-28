#!/usr/bin/env python3
"""FlightBox — the pilot tournament: pit intercept-pilot VARIANTS against each other and read out of
the telemetry WHY one of them won.

A variant is a set of `set pilot_* <value>` lines (systems/FBPilotTuning, doc/missions/combat.md), i.e.
mission DATA — so this tool writes .fbm files and runs the existing headless client. Nothing is
compiled between two candidates and there is no tournament code inside the simulator.

    tools/fb_tournament.py --variants tools/variants-bvr.txt --out /tmp/tourney

WHAT IT RUNS. Every unordered pair of variants, in BOTH seat assignments (A west / B east and B west /
A east), on one starting geometry. Flying both seats is what removes a positional advantage from the
result: the two runs of a pair are mirror images of each other, so the SUM over them measures the
variant and not the seat. Runs go through `fb-gym --threads N` — the simulator's own per-unit
parallelism — and are byte-reproducible whatever N is (that is `--check-determinism`).

THE FITNESS is derived and defended in `Score()` below. In one line: the outcome dominates (+1000 for
a kill, -1200 for being killed), never engaging is punished harder than any craft term can repay, and
everything else can only order runs whose outcome was the same.

Stdlib only, no build target, no dependency on anything under sim/build except the fb-gym binary.
"""

import argparse
import concurrent.futures
import csv
import math
import os
import re
import shutil
import subprocess
import sys

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------------------------
# The arena. Two intercept pilots, identical in everything a mission file can say except the seat
# they sit in and the `pilot_*` lines of their variant. Head-on, co-altitude, co-speed, both outside
# the APG-68's own search gate at t=0, so both runs start with a real search phase and neither side
# is handed a detection. The ONLY asymmetry left in the file is the order of the two blocks, and the
# seat swap cancels that.
# ---------------------------------------------------------------------------------------------
GEOMETRIES = {
    # name: (west lat, west lon, west alt, west kt, east lat, east lon, east alt, east kt)
    "mirror": (46.90, 6.40, 8000, 420, 46.90, 7.70, 8000, 420),
    # The energy split of missions/bvr-duel-decided.fbm: the seat is worth a great deal here, which is
    # precisely why both seats are flown. It asks a different question than `mirror` — not "who wins an
    # even fight" but "who does better from both ends of an uneven one".
    "split": (46.90, 6.40, 12000, 500, 46.90, 7.55, 6000, 350),
}

# THE AIRFRAME BLOCK. A variant names the MODULE it flies, so the same tournament can pit an F-16
# variant against a MiG-29 variant (`module=mig29`). Only the box-specific `set` lines differ, because
# only they name boxes: `fcr_mode crm` means nothing on a jet whose modes are RAD/CC/VS/BORE, and this
# aircraft has no dispenser at all (modules/mig29/module.md gap 4g). Everything the two blocks SHARE —
# the spawn, the master-arm call, `set task intercept`, the vector, the objectives, the `pilot_*`
# lines — is shared text, so a mixed pairing differs from a same-type one in exactly the things the
# two aircraft differ in.
UNIT_TPL = {
    "f16": """unit {call}
  team {team}
{flight}  module f16
  spawn {lat:.5f} {lon:.5f} {alt} {hdg} {kt}
  set gear up
  set fuel_pct 70
  set datalink {dl}
  set fcr_mode crm
  set store 3 aim120
  set store 7 aim120
  set brief_master_arm arm
  set cmds_mode man
  set cmds_program 1
  set cmds_chaff 60
  set cmds_flare 60
{sort}  set task intercept
{tuning}  wp {wplat:.5f} {wplon:.5f} {alt} {kt}
  objective {objective}
  objective survive
""",
    "mig29": """unit {call}
  team {team}
{flight}  module mig29
  spawn {lat:.5f} {lon:.5f} {alt} {hdg} {kt}
  set gear up
  set fuel_pct 70
  set n019_mode rad
  set n019_range_nm 27
  set n019_emission illum
  set rwr on
  set store 3 r27r
  set store 4 r27r
  set brief_master_arm arm
{sort}  set task intercept
{tuning}  wp {wplat:.5f} {wplon:.5f} {alt} {kt}
  objective {objective}
  objective survive
""",
}

# THE COMBAT SPREAD the tournament flies when --flight > 1: one nautical mile abeam, 150 m stacked,
# i.e. exactly FBPilot::FormationSpreadM/FormationStackM (doc/formation.md section 4.2). Written here
# rather than read from anywhere because a tournament arena is mission DATA like every other number
# in this file.
SPREAD_DEG = 1852.0 / 111320.0
STACK_M = 150.0


def unit_block(var, side, call, team, pos, n, lat, lon, alt, hdg, kt, wplat, wplon, foeteam, foe):
    """One actor block. With n == 1 the text is EXACTLY what it was before flights existed — no
    `flight` line, the single-ship `kill unit` objective, the datalink off — so `variants-bvr.txt`
    keeps producing the missions it produced."""
    flight = "" if n == 1 else "  flight %s %d\n" % (side, pos)
    # The station: position 1 on the geometry's own line, each further position one spread to the
    # RIGHT of the leader's track and one stack higher. Right of an easterly heading is south.
    if pos > 1:
        lateral = SPREAD_DEG * (pos - 1)
        lat = lat - lateral if hdg == "90.0" else lat + lateral
        alt = alt + STACK_M * (pos - 1)
    return UNIT_TPL[var.module].format(
        call=call, team=team, flight=flight, lat=lat, lon=lon, alt=alt, hdg=hdg, kt=kt,
        wplat=wplat, wplon=wplon, foe=foe,
        dl=var.dl, sort=("" if not var.sort else "  set brief_sort %s\n" % var.sort),
        objective=("kill unit %s" % foe) if n == 1 else ("kill team %s" % foeteam),
        tuning="".join("  set %s %g\n" % kv for kv in sorted(var.params.items())))


def side_calls(side, n):
    return [side] if n == 1 else ["%s%d" % (side, i + 1) for i in range(n)]


def mission_text(name, timeout, geometry, west_var, east_var, n=1):
    wlat, wlon, walt, wkt, elat, elon, ealt, ekt = GEOMETRIES[geometry]
    blocks = []
    for pos, call in enumerate(side_calls("west", n), 1):
        blocks.append(unit_block(west_var, "west", call, "friendly", pos, n, wlat, wlon, walt, "90.0",
                                 wkt, wlat, wlon + 1.95, "hostile", side_calls("east", n)[0]))
    for pos, call in enumerate(side_calls("east", n), 1):
        blocks.append(unit_block(east_var, "east", call, "hostile", pos, n, elat, elon, ealt, "270.0",
                                 ekt, elat, elon - 1.95, "friendly", side_calls("west", n)[0]))
    return ("name %s\ntimeout %d\n\n# generated by tools/fb_tournament.py — do not edit\n"
            "# west = %s   east = %s   geometry = %s   flight = %d\n\n%s" %
            (name, timeout, west_var.name, east_var.name, geometry, n, "\n".join(blocks)))


class Variant:
    """A variant is a PILOT doctrine plus, since flights exist, a FLIGHT doctrine: whether the element
    uses its cooperative net (`dl=`) and which sort contract it briefed (`sort=`). Both are mission
    data like the `pilot_*` lines, so a flight doctrine is a text line and not a class."""

    def __init__(self, name, params, module="f16", dl="off", sort=""):
        self.name = name
        self.params = params
        self.module = module
        self.dl = dl
        self.sort = sort

    def __repr__(self):
        return "%s[%s](%s)" % (self.name, self.module,
                               ", ".join("%s=%g" % kv for kv in sorted(self.params.items())))


def load_variants(path):
    """One variant per line: `<name> [module=<name>] key=value key=value ...`; '#' comments, blank
    lines ignored. A bare name is the airframe's own numbers (FBF16Pilot / FBMig29Pilot) — the
    baseline every tournament wants. `module=` defaults to f16, so an existing variant file is
    unchanged."""
    out = []
    for lineno, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        tok = line.split()
        params = {}
        module, dl, sort = "f16", "off", ""
        for t in tok[1:]:
            if "=" not in t:
                sys.exit("%s:%d: '%s' is not key=value" % (path, lineno, t))
            k, v = t.split("=", 1)
            if k == "dl":
                if v not in ("on", "off"):
                    sys.exit("%s:%d: dl= wants on|off" % (path, lineno))
                dl = v
                continue
            if k == "sort":
                if v not in ("none", "left", "right", "near", "far"):
                    sys.exit("%s:%d: sort= wants none|left|right|near|far" % (path, lineno))
                sort = "" if v == "none" else v
                continue
            if k == "module":
                if v not in UNIT_TPL:
                    sys.exit("%s:%d: no arena block for module '%s' (have %s)"
                             % (path, lineno, v, "/".join(sorted(UNIT_TPL))))
                module = v
                continue
            if not k.startswith("pilot_"):
                sys.exit("%s:%d: '%s' is not a pilot parameter (they all start with pilot_)" % (path, lineno, k))
            params[k] = float(v)
        if any(v.name == tok[0] for v in out):
            sys.exit("%s:%d: duplicate variant name '%s'" % (path, lineno, tok[0]))
        out.append(Variant(tok[0], params, module, dl, sort))
    if len(out) < 2:
        sys.exit("%s: a tournament needs at least two variants" % path)
    return out


# ---------------------------------------------------------------------------------------------
# Reading a run back
# ---------------------------------------------------------------------------------------------
class Side:
    """One unit's debrief, straight out of its own telemetry file plus the runner's verdict lines."""

    def __init__(self, name):
        self.name = name
        self.result = "NONE"
        self.reason = ""
        self.effective = True
        self.hits = 0
        self.eng = {}
        self.es_start = 0.0

    def g(self, key, default=-1.0):
        v = self.eng.get(key, default)
        return default if v is None else v


def read_side(path, name):
    s = Side(name)
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    head, last = rows[0], rows[-1]
    idx = {h: i for i, h in enumerate(head)}

    def num(row, key):
        try:
            return float(row[idx[key]])
        except (KeyError, ValueError, IndexError):
            return None

    for h in head:
        if h.startswith("eng_") and h != "eng_state":
            s.eng[h] = num(last, h)
    s.eng["eng_state"] = last[idx["eng_state"]] if "eng_state" in idx else "?"
    s.effective = (num(last, "dmg_effective") or 0.0) > 0.5
    s.hits = int(num(last, "dmg_hits") or 0)
    # Energy height at the moment the engagement began, i.e. the first row that is no longer Idle —
    # the reference the energy term is a fraction OF. Falls back to the first row of the run.
    st = idx.get("eng_state")
    es = idx.get("eng_es")
    if st is not None and es is not None:
        s.es_start = float(rows[1][es]) if len(rows) > 1 else 0.0
        for r in rows[1:]:
            if r[st] != "idle":
                s.es_start = float(r[es])
                break
    return s


RESULT_RE = re.compile(r'UNIT_RESULT unit=(\S+) result=(\S+) reason="([^"]*)"')


class FlightView:
    """The opposing FLIGHT as one side's score needs to see it. Only three things are asked of an
    opponent — is it still combat-capable, how much of it was hit, and when it first shot — and all
    three have a flight-level meaning: a flight is capable while ANY member is, hits add up, and the
    first shot is the earliest of them. Every other weight stays a per-member quantity."""

    def __init__(self, members):
        self.name = members[0].name if len(members) == 1 else "%s x%d" % (members[0].name, len(members))
        self.effective = any(m.effective for m in members)
        self.hits = sum(m.hits for m in members)
        shots = [m.g("eng_shot_s") for m in members if m.g("eng_shot_s") >= 0]
        self.eng = {"eng_shot_s": min(shots) if shots else -1.0}

    def g(self, key, default=-1.0):
        v = self.eng.get(key, default)
        return default if v is None else v


def read_run(outdir, west_name, east_name, n=1):
    calls_w, calls_e = side_calls("west", n), side_calls("east", n)
    def path_of(call, idx):
        return os.path.join(outdir, "telemetry.csv" if idx == 0 else "telemetry_%s.csv" % call)
    west = [read_side(path_of(c, i), west_name) for i, c in enumerate(calls_w)]
    east = [read_side(path_of(c, i + len(calls_w)), east_name) for i, c in enumerate(calls_e)]
    by_call = dict(zip(calls_w, west))
    by_call.update(zip(calls_e, east))
    duration = 0.0
    for line in open(os.path.join(outdir, "events.log")):
        m = RESULT_RE.search(line)
        if m and m.group(1) in by_call:
            by_call[m.group(1)].result = m.group(2)
            by_call[m.group(1)].reason = m.group(3)
            by_call[m.group(1)].name = m.group(1)
        if "mission SUMMARY" in line:
            d = re.search(r"durationS=([0-9.]+)", line)
            if d:
                duration = float(d.group(1))
    return west, east, duration


# ---------------------------------------------------------------------------------------------
# THE FITNESS
# ---------------------------------------------------------------------------------------------
# Every weight below is stated with the reason it has that size, and the SIZES are the design: the
# outcome band (+1000 / -1200) is larger than everything else put together, so no amount of good
# craft can out-score a kill and no amount of bad craft can talk one away. The craft terms exist to
# ORDER runs that ended the same way, which — since most BVR engagements between equals end in a
# draw — is most of them.
W_KILL = 1000.0     # the point of the sortie
W_DIED = 1200.0     # ...and losing the jet is worth slightly more than killing one: without the
                    # asymmetry a mutual kill would score the same as a stalemate, and "trade every
                    # time" is then a stable strategy against anybody who does not.
W_HIT = 150.0       # THE ANTI-TRIGGER-HAPPY TERM, and the one that pays for AIM the way the outcome
                    # terms pay for RESULT: a burst that went off on the other aircraft (its own
                    # health register counted it) is worth something even when the jet flew on with
                    # its avionics wrecked, and it is the only way a shot earns anything at all. It
                    # cannot be faked — landing a burst means guiding a round to inside ten metres —
                    # and it is a sixth of a kill, so it can never stand in for one.
W_NOSHOT = 250.0    # THE ANTI-RUNNING-AWAY TERM. A pilot that never fires cannot be killed either,
                    # so a pure survival score has its optimum in leaving the area. This is bigger
                    # than every craft term added together, so running away is never a way to place.
W_SHOT_COST = 25.0  # per round fired: an AIM-120 is not free, and a trigger-happy variant that gets
                    # the same kill with three rounds ranks below one that got it with one.
W_QUALITY = 100.0   # the launch geometry, see below
W_SUPPORT = 80.0    # the discriminator between a launch and a kill
W_LEAD = 40.0       # who got the first shot away (relative to the opponent in the SAME run)
W_DEFENCE = 40.0    # answering a warning that was actually made, and living through it
W_ENERGY = 40.0     # the state the engagement is left in


def shot_quality(s):
    """0..1 for the OPENING shot: where inside the launch zone it was taken, times how far off the
    nose the target was. 1.0 = at or inside Rtr, dead ahead — a shot the target cannot outrun and a
    round that does not have to spend its motor turning. 0.0 = fired at Raero, the maximum kinematic
    range, which is the shot a target defeats by turning around. Between the two it is linear.
    A shot deeper than Rtr does not score higher (the plateau): the SMS refuses inside Rmin anyway,
    and rewarding "closer is better" without limit would pay for pressing in, which the outcome terms
    already price far more heavily."""
    r, rtr, raero = s.g("eng_shot_nm"), s.g("eng_shot_rtr_nm"), s.g("eng_shot_raero_nm")
    if r < 0 or rtr < 0 or raero <= rtr:
        return 0.0
    zone = (raero - r) / (raero - rtr)
    zone = max(0.0, min(1.0, zone))
    ata = abs(s.g("eng_shot_ata", 0.0))
    return zone * max(0.0, math.cos(math.radians(min(ata, 90.0))))


def score(me, foe, duration):
    """One side's fitness for one run, plus the itemised reasons — the report prints the items, not
    just the total, because the point of the tournament is WHY."""
    it = []
    total = 0.0

    def add(label, value, detail):
        nonlocal total
        if abs(value) < 1e-9:
            return
        total += value
        it.append((label, value, detail))

    killed_him = not foe.effective
    i_died = not me.effective
    add("kill", W_KILL if killed_him else 0.0, "%s combat-ineffective" % foe.name)
    add("lost", -W_DIED if i_died else 0.0, "shot down (%d hit(s))" % me.hits)
    add("hits landed", W_HIT * foe.hits, "%d burst(s) on %s" % (foe.hits, foe.name))

    shots = int(me.g("eng_shots", 0.0))
    add("no shot", -W_NOSHOT if shots == 0 else 0.0, "never fired a round")
    add("rounds", -W_SHOT_COST * shots, "%d round(s) fired" % shots)

    q = shot_quality(me)
    add("shot geometry", W_QUALITY * q,
        "opening shot %.1f nm (Rtr %.1f, Raero %.1f), %.0f deg off the nose -> q=%.2f" %
        (me.g("eng_shot_nm"), me.g("eng_shot_rtr_nm"), me.g("eng_shot_raero_nm"),
         abs(me.g("eng_shot_ata", 0.0)), q) if shots else "no shot")

    sup = max(0.0, min(1.0, me.g("eng_support_f", 0.0)))
    add("support", W_SUPPORT * sup,
        "held the uplink %.0f%% of the way to the seeker (pitbull=%d)" % (100 * sup, int(me.g("eng_pitbull", 0.0))))

    # WHO SHOT FIRST, measured against the opponent in this very run — a relative term, which is what
    # a duel is. A side that shot while the other never did gets the full mark; the tanh keeps a
    # fifteen-second head start from being worth ten times a five-second one.
    mine, theirs = me.g("eng_shot_s"), foe.g("eng_shot_s")
    if mine >= 0 or theirs >= 0:
        a = mine if mine >= 0 else duration
        b = theirs if theirs >= 0 else duration
        lead = math.tanh((b - a) / 15.0)
        add("shot lead", W_LEAD * lead,
            "first shot at t=%.1f vs %.1f" % (a, b) if mine >= 0 and theirs >= 0
            else ("shot while %s never did" % foe.name if mine >= 0 else "never shot, %s did" % foe.name))

    # DEFENCE is only scored where there was something to defend against, and only if it worked. It
    # cannot be farmed: being shot at is the opponent's decision, not this pilot's, and the term is
    # capped well below what one kill is worth.
    react = me.g("eng_react_s")
    if me.g("eng_threat_s") >= 0 and not i_died:
        d = max(0.0, 1.0 - react / 8.0) if react >= 0 else 0.0
        add("defence", W_DEFENCE * d,
            "answered the warning in %.1f s and lived" % react if react >= 0 else "never answered the warning")

    # ENERGY: the low-water mark of energy height as a fraction of what it started the engagement
    # with. Small on purpose — flying straight and level is perfect on this axis and hopeless on
    # every other one.
    if me.es_start > 1.0:
        frac = max(0.0, min(1.0, me.g("eng_es_min", 0.0) / me.es_start))
        add("energy", W_ENERGY * frac, "energy floor %.0f%% of entry (%.0f -> %.0f ft)" %
            (100 * frac, me.es_start, me.g("eng_es_min", 0.0)))
    return total, it


# ---------------------------------------------------------------------------------------------
# Running
# ---------------------------------------------------------------------------------------------
def run_one(job):
    gym, outroot, tag, text, threads = job
    d = os.path.join(outroot, tag)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "duel.fbm")
    with open(path, "w") as f:
        f.write(text)
    r = subprocess.run([gym, "--mission", path, "--out", d, "--threads", str(threads)],
                       cwd=SIM_DIR, capture_output=True, text=True)
    if not any(f.startswith("telemetry_") for f in os.listdir(d)):
        sys.exit("run %s produced no telemetry:\n%s%s" % (tag, r.stdout[-2000:], r.stderr[-2000:]))
    return tag, r.returncode




# =================================================================================================
# THE ATTRIBUTION TEST — doc/modules/air/module.md §Spec 11, and the round's real acceptance.
#
# A catalogue opponent that decides a campaign BY BEING BADLY MODELLED turns every campaign result into
# a measurement of our own error. Three instruments answer "did he lose as a MiG-21, or as a coarse
# deck?", and the two this file carries are:
#
#   band_deck      the outcome spread when the row's DECLARED-IGNORANCE deck values are perturbed
#                  (CD0 +-10 %, e +-10 %, Ixx +-10 %, thrust +-5 %) with the doctrine held FIXED
#   band_doctrine  the outcome spread when the mission's DOCTRINE levers are swept with the deck held
#                  fixed — the O1 yardstick, band = max_v O(v) - min_v O(v)
#
# THE VERDICT RULE: the row is admissible as an opponent iff band_deck <= 0.25 * band_doctrine. If
# perturbing what we ADMIT WE DO NOT KNOW moves the result as far as changing the doctrine does, the
# campaign is measuring the model — and this tool then PRINTS BOTH BANDS INSTEAD OF A RESULT. That is
# built as the behaviour and not as a warning: `Attribution.result` is None below the threshold, and the
# caller has nothing to print but the two numbers.
#
# THE THRESHOLD 0.25 IS [SET]: no source gives one, a quarter keeps the deck's contribution below the
# doctrine's even when both extremes land the same way, and BOTH BANDS ARE PRINTED beside every result,
# so a reader who disagrees with the number can re-decide without re-running anything.
#
# THE CONTROL CELL is the falsification: the same geometry flown with the row's deck REPLACED by the
# pinned F-16 deck, keeping the row's sensors, weapons and pilot tier. If the outcome does not move, the
# deck was not what decided and instrument 2 must have said so. A DISAGREEMENT BETWEEN THE TWO IS A
# DEFECT OF THE INSTRUMENT, NOT A FINDING.
# =================================================================================================

# The deck perturbations, each naming the recipe quantity it moves.
DECK_PERTURBATIONS = [
    ("baseline", {}),
    ("cd0-10%", {"cd0": 0.90}),
    ("cd0+10%", {"cd0": 1.10}),
    ("e-10%", {"e": 0.90}),
    ("e+10%", {"e": 1.10}),
    ("ixx-10%", {"ixx": 0.90}),
    ("ixx+10%", {"ixx": 1.10}),
    ("thrust-5%", {"thrust": 0.95}),
    ("thrust+5%", {"thrust": 1.05}),
]

# The doctrine sweep, held on ONE side (the catalogue row's) so the two bands answer about the same
# aircraft. Every lever is an existing `set pilot_*` key — mission data, not model.
DOCTRINE_VARIANTS = [
    ("baseline", {}),
    ("shoot-early", {"pilot_shot_rtr": 1.6}),
    ("shoot-late", {"pilot_shot_rtr": 0.7}),
    ("commit-far", {"pilot_lock_nm": 30.0}),
    ("commit-near", {"pilot_lock_nm": 8.0}),
    ("react-fast", {"pilot_react_s": 1.0}),
    ("react-slow", {"pilot_react_s": 8.0}),
    ("beam-hard", {"pilot_beam_deg": 120.0}),
    ("press-on", {"pilot_abort_nm": 1.0}),
]

# The arena the two bands are measured on: the catalogue row against one F-16, head-on, co-altitude,
# both outside their own search gates at t=0. The F-16 side never changes, so every point of both bands
# differs from every other in exactly one declared quantity.
ATTR_TPL = """name attr-{tag}
timeout 300

unit bandit
  team hostile
  module {row}
  spawn 46.90000 7.70000 8000 270.0 450
  set gear up
  set task intercept
{stores}  set brief_master_arm arm
{tuning}  wp 46.90000 5.90000 8000 450
  objective survive

unit viper
  team friendly
  module {control}
  spawn 46.90000 6.40000 8000 90.0 450
  set gear up
  set fuel_pct 70
  set datalink off
  set fcr_mode crm
  set store 3 aim120
  set store 7 aim120
  set brief_master_arm arm
  set task intercept
  wp 46.90000 8.35000 8000 450
  objective kill unit bandit
  objective survive
"""

# What each catalogue row shoots, from its own catalogue entry — held FIXED across both bands, because
# the control cell's whole point is that only the DECK changes.
ROW_STORES = {
    "mig17": "",
    "mig21": "  set store 1 r60\n  set store 2 r60\n",
    "mig23": "  set store 1 r24r\n  set store 2 r24r\n",
    "mig25": "  set store 1 r40r\n  set store 2 r40r\n",
    "su7": "",
    "su22": "  set store 1 r60\n  set store 2 r60\n",
    "mirf1": "  set store 1 s530f\n  set store 2 magic1\n",
    "f5e": "  set store 1 aim9\n  set store 2 aim9\n",
    "f15c": "  set store 1 aim7\n  set store 2 aim7\n",
    "su27": "  set store 1 r27r\n  set store 2 r27r\n",
}


def attr_mission(tag, row, params, control=None):
    return ATTR_TPL.format(
        tag=tag, row=row, control=control or "f16", stores=ROW_STORES.get(row, ""),
        tuning="".join("  set %s %g\n" % kv for kv in sorted(params.items())))


# THE CONTROL CELL'S deck donor. A module key binds a deck AND its sensors, so the cell cannot be flown
# by swapping the module — that would change the sensors and the weapons too, and the whole point is
# that only the DECK moves. It is done by REGENERATING the row's own XML from ANOTHER row's anchors and
# leaving the catalogue entry alone: same key, same radar, same rounds, same tier, a different
# aeroplane underneath. The donor is `f15c`, whose deck is the recipe's cleanest turbofan row.
CONTROL_DONOR = "f15c"


def attr_outcome(outdir):
    """ONE number per run, and it is the SAME fitness the tournament ranks on — so a band computed here
    and a duel result printed there are the same quantity. The catalogue row's score."""
    try:
        bandit = read_side(os.path.join(outdir, "telemetry.csv"), "bandit")
        viper = read_side(os.path.join(outdir, "telemetry_viper.csv"), "viper")
    except (OSError, IndexError):
        return None
    duration = 0.0
    ev = os.path.join(outdir, "events.log")
    if os.path.exists(ev):
        for line in open(ev):
            m = RESULT_RE.search(line)
            if m and m.group(1) in ("bandit", "viper"):
                (bandit if m.group(1) == "bandit" else viper).result = m.group(2)
            if "mission SUMMARY" in line:
                d = re.search(r"durationS=([0-9.]+)", line)
                if d:
                    duration = float(d.group(1))
    return score(bandit, viper, duration)[0]


def run_attr(gym, outroot, tag, text, deck=None, row=None):
    """One arena run. `deck` is the perturbation dict; regenerating the row's deck IN PLACE is the only
    way to change a JSBSim model this tree offers (one model root, fdm/FBModelRoots.h), so the caller
    MUST restore it afterwards — attribution() does, unconditionally."""
    if deck:
        cmd = [sys.executable, os.path.join(SIM_DIR, "tools", "gen_air_decks.py"), "--only", row]
        for k, v in sorted(deck.items()):
            cmd += ["--" + k, str(v)]
        subprocess.run(cmd, cwd=SIM_DIR, capture_output=True, text=True, check=True)
    d = os.path.join(outroot, tag)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "attr.fbm")
    with open(path, "w") as f:
        f.write(text)
    subprocess.run([gym, "--mission", path, "--out", d, "--threads", "1"], cwd=SIM_DIR,
                   capture_output=True, text=True)
    return attr_outcome(d)


def restore_decks():
    """Put the asset tree back EXACTLY as the recipe writes it. The directories are removed first,
    because the control cell writes a DONOR's engine file into the target row's directory and a plain
    regeneration would leave it behind — measured: `tools/gen_air_decks.py --check` failed on three rows
    after the first attribution run, with an F100 deck sitting next to a turbojet."""
    for key in ROW_STORES:
        shutil.rmtree(os.path.join(SIM_DIR, "assets", "aircraft", key), ignore_errors=True)
    subprocess.run([sys.executable, os.path.join(SIM_DIR, "tools", "gen_air_decks.py")],
                   cwd=SIM_DIR, capture_output=True, text=True, check=True)


def attribution(gym, outroot, row, threshold=0.25):
    try:
        deck_pts, doc_pts = [], []
        for name, pert in DECK_PERTURBATIONS:
            o = run_attr(gym, outroot, "%s-deck-%s" % (row, name), attr_mission(row + name, row, {}),
                         pert, row)
            deck_pts.append((name, o))
        restore_decks()
        for name, params in DOCTRINE_VARIANTS:
            o = run_attr(gym, outroot, "%s-doc-%s" % (row, name),
                         attr_mission(row + name, row, params))
            doc_pts.append((name, o))
        subprocess.run([sys.executable, os.path.join(SIM_DIR, "tools", "gen_air_decks.py"),
                        "--only", CONTROL_DONOR, "--as", row], cwd=SIM_DIR, capture_output=True,
                       text=True, check=True)
        control = run_attr(gym, outroot, "%s-control" % row, attr_mission(row + "control", row, {}))
    finally:
        restore_decks()

    dv = [o for _, o in deck_pts if o is not None]
    ov = [o for _, o in doc_pts if o is not None]
    band_deck = (max(dv) - min(dv)) if dv else float("nan")
    band_doc = (max(ov) - min(ov)) if ov else float("nan")
    base = dict(deck_pts).get("baseline")

    print("\n" + "=" * 96)
    print("ATTRIBUTION — %s   (module.md §Spec 11: did he lose as a %s, or as a coarse deck?)" % (row, row))
    print("=" * 96)
    print("  deck perturbations (doctrine FIXED) — the four quantities the recipe declares it does not know")
    for name, o in deck_pts:
        print("    %-12s %s" % (name, "-" if o is None else "%+9.1f" % o))
    print("  doctrine sweep (deck FIXED) — every lever is an existing `set pilot_*` key")
    for name, o in doc_pts:
        print("    %-12s %s" % (name, "-" if o is None else "%+9.1f" % o))
    print("  control cell (the row's DECK replaced by the pinned F-16's, sensors/weapons/tier held): %s"
          % ("-" if control is None else "%+.1f" % control))
    print("  ---------------------------------------------------------------------------")
    print("  band_deck      = %10.1f" % band_deck)
    print("  band_doctrine  = %10.1f" % band_doc)
    ratio = band_deck / band_doc if band_doc > 1e-9 else float("inf")
    print("  ratio          = %10.3f   (admissible iff <= %.2f)" % (ratio, threshold))
    if band_doc > 1e-9 and ratio <= threshold:
        print("  VERDICT: ADMISSIBLE — the campaign is measuring the doctrine, and the result is")
        print("           %+.1f, printed WITH both bands beside it." % (base if base is not None else float("nan")))
        return 0
    print("  VERDICT: NOT ADMISSIBLE AS A CAMPAIGN OPPONENT. Perturbing what we admit we do not know")
    print("           moves the outcome as far as changing the doctrine does, so this file prints THE")
    print("           TWO BANDS INSTEAD OF A RESULT. There is no result line above and that is the")
    print("           output, not an omission.")
    return 1


def main():
    ap = argparse.ArgumentParser(description="pilot-variant tournament (see the module docstring)")
    ap.add_argument("--attribution", action="append", default=[],
                    help="run doc/modules/air/module.md §Spec 11's two-band attribution test for this "
                         "catalogue row instead of a tournament (repeatable)")
    ap.add_argument("--variants", help="variant file: '<name> pilot_key=value ...' per line")
    ap.add_argument("--out", required=True, help="output directory (one sub-directory per run)")
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--geometry", default="mirror", choices=sorted(GEOMETRIES))
    ap.add_argument("--flight", type=int, default=1, choices=(1, 2, 4),
                    help="element size per side: 1 = the single-ship arena (unchanged), 2/4 = a FLIGHT "
                         "of that many, all flying the same variant, in combat spread, with the "
                         "objective changed from `kill unit` to `kill team`")
    ap.add_argument("--timeout", type=int, default=260, help="sim-seconds per run")
    ap.add_argument("--threads", type=int, default=2, help="fb-gym --threads (per-unit step parallelism)")
    ap.add_argument("--jobs", type=int, default=4, help="runs in flight at once (separate processes)")
    ap.add_argument("--check-determinism", action="store_true",
                    help="re-fly every pairing at --threads 1 and 2 and compare the telemetry byte for byte")
    args = ap.parse_args()

    if args.attribution:
        os.makedirs(args.out, exist_ok=True)
        rc = 0
        for row in args.attribution:
            rc |= attribution(args.gym, args.out, row)
        return rc
    if not args.variants:
        sys.exit("--variants is required unless --attribution is given")

    variants = load_variants(args.variants)
    os.makedirs(args.out, exist_ok=True)

    jobs, meta = [], []
    for i in range(len(variants)):
        for j in range(i + 1, len(variants)):
            for west, east in ((variants[i], variants[j]), (variants[j], variants[i])):
                tag = "%s_vs_%s" % (west.name, east.name)
                text = mission_text(tag, args.timeout, args.geometry, west, east, args.flight)
                jobs.append((args.gym, args.out, tag, text, args.threads))
                meta.append((tag, west, east))

    print("tournament: %d variants, %d pairings, %d runs, geometry=%s, flight=%d, timeout=%ds, threads=%d\n"
          % (len(variants), len(jobs) // 2, len(jobs), args.geometry, args.flight, args.timeout,
             args.threads))
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        exits = dict(ex.map(run_one, jobs))

    if args.check_determinism:
        alt = [(args.gym, args.out + "-det", t, x, 1) for (_, _, t, x, _) in jobs]
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
            list(ex.map(run_one, alt))

    totals = {v.name: 0.0 for v in variants}
    outcomes = {v.name: 0.0 for v in variants}       # the kill/lost/hit part of the score, alone
    runs = {v.name: 0 for v in variants}
    record = {v.name: [0, 0, 0] for v in variants}   # kills, losses, draws
    print("=" * 100)
    print("PAIRINGS — every run, both seat assignments, with the itemised reason for each side")
    print("=" * 100)
    for tag, west, east in meta:
        wm, em, duration = read_run(os.path.join(args.out, tag), west.name, east.name, args.flight)
        wf, ef = FlightView(wm), FlightView(em)
        # A FLIGHT's fitness is the SUM over its members, each scored against the opposing FLIGHT.
        # With --flight 1 that is one member against one member, i.e. the previous arithmetic exactly.
        wsi = [score(m, ef, duration) for m in wm]
        esi = [score(m, wf, duration) for m in em]
        ws, es = sum(x[0] for x in wsi), sum(x[0] for x in esi)
        totals[west.name] += ws
        totals[east.name] += es
        runs[west.name] += 1
        runs[east.name] += 1
        for pack, who in ((wsi, west.name), (esi, east.name)):
            for _, items in pack:
                outcomes[who] += sum(v for l, v, _ in items if l in ("kill", "lost", "hits landed"))
        for me, foe, who in ((wf, ef, west.name), (ef, wf, east.name)):
            if not foe.effective and me.effective:
                record[who][0] += 1
            elif not me.effective:
                record[who][1] += 1
            else:
                record[who][2] += 1
        print("\n%-28s exit=%d  %.1f sim-s" % (tag, exits[tag], duration))
        rows = [(m, west, sc, items) for m, (sc, items) in zip(wm, wsi)] + \
               [(m, east, sc, items) for m, (sc, items) in zip(em, esi)]
        for side, var, sc, items in rows:
            print("  %-6s %-14s %-8s %-42s score %+8.1f" %
                  (side.name, var.name, side.result, side.reason[:42], sc))
            print("       detect %5s  lock %5s  shot %5s  supp %4s  threat %5s  react %5s  defend %5s  shots %d  chaff %d"
                  % (fmt(side.g("eng_detect_s")), fmt(side.g("eng_lock_s")), fmt(side.g("eng_shot_s")),
                     ("%.2f" % side.g("eng_support_f", 0.0)), fmt(side.g("eng_threat_s")),
                     fmt(side.g("eng_react_s")), ("%.1f" % side.g("eng_defend_s", 0.0)),
                     int(side.g("eng_shots", 0.0)), int(side.g("eng_chaff", 0.0))))
            for label, value, detail in items:
                print("       %+8.1f  %-14s %s" % (value, label, detail))

    print("\n" + "=" * 100)
    print("RANKING — fitness is the MEAN over that variant's runs, so it does not grow with the field")
    print("size; `outcome` is the kill/loss/hit part of it alone, `craft` the rest (the part that only")
    print("orders runs which ended the same way).")
    print("=" * 100)
    print("%-16s %9s %9s %8s   %4s %4s %4s   %s" %
          ("variant", "fitness", "outcome", "craft", "kill", "lost", "draw", "parameters"))
    for v in sorted(variants, key=lambda x: -totals[x.name] / max(1, runs[x.name])):
        k, l, d = record[v.name]
        n = max(1, runs[v.name])
        print("%-16s %9.1f %9.1f %8.1f   %4d %4d %4d   %s" %
              (v.name, totals[v.name] / n, outcomes[v.name] / n, (totals[v.name] - outcomes[v.name]) / n,
               k, l, d, " ".join("%s=%g" % kv for kv in sorted(v.params.items())) or "(airframe defaults)"))

    if args.check_determinism:
        bad = 0
        for tag, _, _ in meta:
            files = ["telemetry.csv"] + ["telemetry_%s.csv" % c for c in
                     (side_calls("west", args.flight)[1:] + side_calls("east", args.flight))]
            for f in files:
                a = open(os.path.join(args.out, tag, f), "rb").read()
                b = open(os.path.join(args.out + "-det", tag, f), "rb").read()
                if a != b:
                    print("DETERMINISM FAIL: %s/%s differs between --threads %d and 1" % (tag, f, args.threads))
                    bad += 1
        print("\ndeterminism: %d file(s) differ between --threads %d and --threads 1 (%d pairings)"
              % (bad, args.threads, len(meta)))
        if bad:
            return 1
    return 0


def fmt(v):
    return "-" if v is None or v < 0 else "%.1f" % v


if __name__ == "__main__":
    sys.exit(main())
