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

THE FITNESS lives in `tools/fb_fitness.py` and is LEXICOGRAPHIC: (V, M, C) — the judge's verdict, then
how many of its own declared objectives the unit met, then craft. Compared left to right, so no craft
value can ever cross a level, and a variant's score is a normalised WIN RATE over pairwise domination
rather than a mean of numbers in a currency that has no units. doc/doctrine-evolution.md §1.

Stdlib only, no build target, no dependency on anything under sim/build except the fb-gym binary.
"""

import argparse
import concurrent.futures
import math
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_fitness as fit

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------------------------
# The arena. Two intercept pilots, identical in everything a mission file can say except the seat
# they sit in and the `pilot_*` lines of their variant. Head-on, co-altitude, co-speed, both outside
# the APG-68's own search gate at t=0, so both runs start with a real search phase and neither side
# is handed a detection. The ONLY asymmetry left in the file is the order of the two blocks, and the
# seat swap cancels that.
# ---------------------------------------------------------------------------------------------
GEOMETRIES = {
    # A geometry is a QUESTION, and doc/doctrine-evolution.md §4 says which questions an arena has to
    # be able to ask before anything measured on it means anything. Seat = (lat, lon, alt m, kt, hdg)
    # plus, where the question needs a different aeroplane, the module that seat flies.
    #
    # THE ARENA IS ITS OWN MEASUREMENT: `tools/fb_arena_check.py` flies the fixed yardstick over every
    # entry here and prints which of them are INFORMATIVE. The saturated ones are kept on purpose —
    # S5 is a yield criterion and a yield needs a denominator, and two of them are the tree's own
    # published findings about geometries that decide nothing (`mirror`, `offset`).
    #
    # --- ASPECT ------------------------------------------------------------------------------------
    # Head-on, co-altitude, co-speed, both outside the APG-68's own search gate at t=0, so both runs
    # start with a real search phase and neither side is handed a detection. [MESS] SATURATED: the
    # symmetric F-16 field draws every cell (doc/duels.md, 30 runs, 0 kills, 0 losses).
    "mirror": ((46.90, 6.40, 8000, 420, 90.0), (46.90, 7.70, 8000, 420, 270.0)),
    # 50 deg of crossing. [MESS] SATURATED and known to be: a crossing target spends the round's energy
    # on turning, and the geometry draws every doctrine cell head-on decides (doc/duels.md row 2).
    "offset": ((46.90, 6.40, 8000, 420, 90.0), (46.55, 7.60, 8000, 420, 320.0)),
    # Stern conversion — the one aspect with no head-on phase at all: both run east, the west jet 60 kt
    # faster, so the whole engagement lives in somebody's rear hemisphere.
    "stern": ((46.90, 6.40, 8000, 480, 90.0), (46.90, 7.70, 8000, 420, 90.0)),
    #
    # --- ENERGY ------------------------------------------------------------------------------------
    # The energy split of missions/bvr-duel-decided.fbm: the seat is worth a great deal here, which is
    # precisely why both seats are flown. Not "who wins an equal fight" but "who does better from both
    # ends of an unequal one".
    "split": ((46.90, 6.40, 12000, 500, 90.0), (46.90, 7.55, 6000, 350, 270.0)),
    #
    # --- DETECTION SYMMETRY ------------------------------------------------------------------------
    # The long approach: the search gate is crossed with 190 km of run-in, so a doctrine has time to be
    # wrong twice. The opposite question to `close` below.
    "far": ((46.90, 5.80, 8000, 420, 90.0), (46.90, 8.30, 8000, 420, 270.0)),
    #
    # --- WEAPON OBLIGATION -------------------------------------------------------------------------
    # Three of the five axes at once — the long run-in, the energy split and the weapon obligation —
    # and it is the ONLY cell in this table that a GENOME allele moves the outcome class on. [MESS,
    # --flight 2, tools/levers-genome.txt] 3 movers of 9, all three of G3's alleles, over the classes
    # (3,1)/(4,2)/(6,4); the fixed yardstick spreads 4 classes at a 48.3 % modal share. Seventeen
    # candidates were screened for this and sixteen returned 0 or 1 mover, so it is a find and not a
    # family: doc/doctrine-evolution.md §State "The arena under the genome's own alphabet".
    "xfarsplit": ((46.90, 5.80, 12000, 500, 90.0), (46.90, 8.30, 6000, 350, 270.0, "mig29")),
    # The axis with no other spelling: it is a property of the AIRCRAFT in the seat. [MESS] AIM-120
    # 0.3 s of binding against R-27R 17.3 s, a factor of 58 (doc/formation.md §6), and the mixed
    # tournament decides 12 of 30 runs where the symmetric one decides none (doc/duels.md).
    "xmirror": ((46.90, 6.40, 8000, 420, 90.0), (46.90, 7.70, 8000, 420, 270.0, "mig29")),
    # ...the same asymmetry with both jets already INSIDE each other's gate: no search phase, the fight
    # starts at the commit.
    "xclose": ((46.90, 6.95, 8000, 420, 90.0), (46.90, 7.30, 8000, 420, 270.0, "mig29")),
    # ...and with the energy split under it, so the two asymmetries are also asked together.
    "xsplit": ((46.90, 6.40, 12000, 500, 90.0), (46.90, 7.55, 6000, 350, 270.0, "mig29")),
    #
    # --- THE MERGE ---------------------------------------------------------------------------------
    # The only geometries that ENTER close combat, and they must say so in the profile: the intercept
    # phase turns around at 5 nm and there is no transition from it into Phase::Bfm anywhere in
    # FBPilot. 8 km apart, head-on, co-altitude, both on `set task bfm` — doc/missions/duel-merge.fbm's
    # own setup, which is where these two numbers come from.
    "merge": ((46.90, 6.95, 5000, 380, 90.0), (46.90, 7.055, 5000, 380, 270.0), "merge"),
    # ...and the mixed one, where the MiG's weapon set is not the inferior one (R-73 off the helmet,
    # 2.7x the round energy, 24.2 deg/s against 16.2).
    "xmerge": ((46.90, 6.95, 5000, 380, 90.0), (46.90, 7.055, 5000, 380, 270.0, "mig29"), "merge"),
    # ...the mixed merge entered with 2,000 m and 70 kt in hand: the one merge cell in which a lever
    # moves the outcome class at all. [MESS] 2 movers of 9 on tools/levers-merge.txt, and BOTH are the
    # energy gene's two rails. READ THE §State ROW BEFORE USING IT: what it decides is a MiG-29 CFIT
    # (9 of 11 east results), not a gunfight — 70 merge runs produced 6 gun bursts and 0 hits.
    "xmergesplit": ((46.90, 6.95, 7000, 450, 90.0), (46.90, 7.055, 5000, 380, 270.0, "mig29"), "merge"),
}


# THE AIRFRAME BLOCK. A variant names the MODULE it flies, so the same tournament can pit an F-16
# variant against a MiG-29 variant (`module=mig29`). Only the box-specific `set` lines differ, because
# only they name boxes: `fcr_mode crm` means nothing on a jet whose modes are RAD/CC/VS/BORE, and this
# aircraft has no dispenser at all (modules/mig29/module.md gap 4g). Everything the two blocks SHARE —
# the spawn, the master-arm call, `set task intercept`, the vector, the objectives, the `pilot_*`
# lines — is shared text, so a mixed pairing differs from a same-type one in exactly the things the
# two aircraft differ in.
#
# THE SECOND PROFILE IS THE MERGE. A geometry that asks a close-combat question cannot ask it with
# `set task intercept`: FBPilot has exactly ONE transition into Phase::Bfm and it is the briefed task
# at spawn — the intercept phase turns AROUND at InterceptAbortRangeNm (5 nm) and never merges. The
# `merge` blocks are doc/missions/duel-merge.fbm's own box settings (ACM HUD / N019 ACM via the BFM
# pilot, the WVR round on the rails, chaff and flares loaded) with the tournament's flight, sort,
# tuning and objective machinery around them. The `wp` is not steered to — a fight has no waypoint —
# it is the nav solution the fire-control block gates the EEGS gun funnel on (doc/missions/gun-bfm.fbm).
#
# AND IT BRIEFS THE GUN CONTROL POSITION, because the gun is the ONLY weapon Phase::Bfm can employ:
# FBPilot's fight tick ends in BfmGunfire and there is no WVR missile shot anywhere in it. The default
# band (0.5-1.5 nm) is a MISSILE holding position and lies outside the EEGS funnel (600-3,000 ft), so a
# merge briefed with it flies a perfect tail chase that can never squeeze — [MESS, xmergesplit] the MiG
# held 132.4 s of control position at a median 2.64 nm and |ata| 0.4 deg, `gun_in_funnel` 0 in 4,200
# ticks, 0 triggers. The two numbers are missions/gun-bfm.fbm's own, and its header carries the rule.
# [MESS, same geometry, only these two lines added] 63 bursts and 23 `gun HIT` lines against 0 and 0.
UNIT_TPL = {
  "bvr": {
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
  },
  "merge": {
    "f16": """unit {call}
  team {team}
{flight}  module f16
  spawn {lat:.5f} {lon:.5f} {alt} {hdg} {kt}
  set gear up
  set fuel_pct 60
  set datalink {dl}
  set fcr_mode acm_hud
  set fcr_range_nm 10
  set rwr on
  set store 3 aim9
  set store 7 aim9
  set brief_master_arm arm
  set cmds_mode man
  set cmds_program 1
  set cmds_chaff 60
  set cmds_flare 60
{sort}  set task bfm
  set pilot_bfm_ctrl_min_nm 0.15
  set pilot_bfm_ctrl_max_nm 0.40
{tuning}  wp {wplat:.5f} {wplon:.5f} {alt} {kt}
  objective {objective}
  objective survive
""",
    "mig29": """unit {call}
  team {team}
{flight}  module mig29
  spawn {lat:.5f} {lon:.5f} {alt} {hdg} {kt}
  set gear up
  set fuel_pct 60
  set n019_mode rad
  set n019_range_nm 10
  set n019_emission illum
  set rwr on
  set store 3 r73
  set store 4 r73
  set brief_master_arm arm
  set cmds_mode man
  set cmds_chaff 30
  set cmds_flare 30
{sort}  set task bfm
  set pilot_bfm_ctrl_min_nm 0.15
  set pilot_bfm_ctrl_max_nm 0.40
{tuning}  wp {wplat:.5f} {wplon:.5f} {alt} {kt}
  objective {objective}
  objective survive
""",
  },
}

# THE COMBAT SPREAD the tournament flies when --flight > 1: one nautical mile abeam, 150 m stacked,
# i.e. exactly FBPilot::FormationSpreadM/FormationStackM (doc/formation.md section 4.2). Written here
# rather than read from anywhere because a tournament arena is mission DATA like every other number
# in this file.
SPREAD_DEG = 1852.0 / 111320.0
STACK_M = 150.0


def unit_block(var, side, call, team, pos, n, lat, lon, alt, hdg, kt, wplat, wplon, foeteam, foe,
               profile="bvr"):
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
    return UNIT_TPL[profile][var.module].format(
        call=call, team=team, flight=flight, lat=lat, lon=lon, alt=alt, hdg=hdg, kt=kt,
        wplat=wplat, wplon=wplon, foe=foe,
        dl=var.dl, sort=("" if not var.sort else "  set brief_sort %s\n" % var.sort),
        objective=("kill unit %s" % foe) if n == 1 else ("kill team %s" % foeteam),
        tuning="".join("  set %s %g\n" % kv for kv in sorted(var.params.items())))


def side_calls(side, n):
    return [side] if n == 1 else ["%s%d" % (side, i + 1) for i in range(n)]


# The briefed vector: 1.95 deg along the spawn heading, in plain degrees. At 90/270 that is exactly the
# +-1.95 deg of longitude the two original geometries were written with, so their mission text is
# unchanged to the byte; every other heading follows the same rule instead of getting a special case.
def vector_of(lat, lon, hdg):
    return (lat + 1.95 * math.cos(math.radians(hdg)), lon + 1.95 * math.sin(math.radians(hdg)))


def seat(spec):
    """A seat is (lat, lon, alt, kt, hdg) and, where the arena asks a question only a DIFFERENT
    AIRFRAME can ask, a sixth entry naming the module that seat flies. doc/doctrine-evolution.md §4.2's
    fifth axis — weapon obligation, 0.3 s against 17.3 s of binding — has no other spelling: it is a
    property of the aircraft in the seat, not of where the seat is."""
    return spec[:5], (spec[5] if len(spec) > 5 else None)


def profile_of(geometry):
    """`bvr` unless the geometry names its own — a geometry is a QUESTION and the phase the pilots fly
    it in is part of the question, not a flag on the runner."""
    g = GEOMETRIES[geometry]
    return g[2] if len(g) > 2 else "bvr"


def mission_text(name, timeout, geometry, west_var, east_var, n=1):
    wspec, espec = GEOMETRIES[geometry][:2]
    profile = profile_of(geometry)
    (wlat, wlon, walt, wkt, whdg), wmod = seat(wspec)
    (elat, elon, ealt, ekt, ehdg), emod = seat(espec)
    if wmod:
        west_var = Variant(west_var.name, west_var.params, wmod, west_var.dl, west_var.sort)
    if emod:
        east_var = Variant(east_var.name, east_var.params, emod, east_var.dl, east_var.sort)
    wvec, evec = vector_of(wlat, wlon, whdg), vector_of(elat, elon, ehdg)
    blocks = []
    for pos, call in enumerate(side_calls("west", n), 1):
        blocks.append(unit_block(west_var, "west", call, "friendly", pos, n, wlat, wlon, walt,
                                 "%.1f" % whdg, wkt, wvec[0], wvec[1], "hostile",
                                 side_calls("east", n)[0], profile))
    for pos, call in enumerate(side_calls("east", n), 1):
        blocks.append(unit_block(east_var, "east", call, "hostile", pos, n, elat, elon, ealt,
                                 "%.1f" % ehdg, ekt, evec[0], evec[1], "friendly",
                                 side_calls("west", n)[0], profile))
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
                if v not in UNIT_TPL["bvr"]:
                    sys.exit("%s:%d: no arena block for module '%s' (have %s)"
                             % (path, lineno, v, "/".join(sorted(UNIT_TPL["bvr"]))))
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
# Reading a run back — every reader and the whole fitness live in tools/fb_fitness.py, so exactly one
# of each exists and the evolution runner shares them rather than copying them.
# ---------------------------------------------------------------------------------------------
def read_run(outdir, west_name, east_name, n=1):
    return fit.read_pair(outdir, side_calls("west", n), side_calls("east", n), west_name, east_name)


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
# THE CONTROL CELL is the falsification: the same geometry flown with the row's deck REPLACED by a
# DONOR row's, keeping the row's sensors, weapons and pilot tier. §Spec 11's rule is ONE-SIDED and the
# one-sidedness is the whole content: *if the outcome does not move, the deck was not what decided and
# instrument 2 must have said so.*
#
# THE ROUND THAT BUILT THIS FILE READ THAT RULE TWO-SIDEDLY AND RECORDED A DEFECT THAT IS NOT ONE.
# On `mig21` it saw band_deck = 2.4 beside a control-cell displacement of 1 203.6 and called the pair a
# contradiction (module.md `## State` B5). It is not. The two numbers do not measure the same thing and
# were never meant to:
#
#   band_deck            the spread over +-10 % of four DECLARED-IGNORANCE quantities — a DIFFERENTIAL
#                        sensitivity, in outcome points per "how wrong we admit we might be"
#   control displacement the outcome difference to A DIFFERENT AEROPLANE — 2.4x the mass, 2.5x the wing
#                        and 2.7x the thrust of the subject. A FINITE, unbounded difference
#
# A small differential beside a large finite difference is the HEALTHY signature: our uncertainty does
# not move the result, a categorically different airframe does. The defect §Spec 11 names is the OTHER
# combination — a control cell that moves the outcome NO FURTHER than our own +-10 % does, because then
# the geometry does not discriminate decks at all and NEITHER band means anything. This file now
# EVALUATES that condition instead of printing the cell's number and leaving the reading to a reader.
#
# AND THE TWO ARE MADE COMMENSURABLE by the one ratio that has a meaning: band_deck / |control - base|,
# i.e. our declared ignorance expressed as a fraction of a whole different aeroplane.
#
# WHERE NO DONOR EXISTS THERE IS NO CELL. The donor is one fixed reference deck, so for the DONOR ROW
# ITSELF the cell is the baseline and a number computed from it would be a no-op dressed as a
# measurement. The rule is stated and the cell is reported as `n/a`, never as 0.0.
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
# aeroplane underneath. The donor is `f15c`, whose deck is the recipe's cleanest turbofan row and one of
# the four that are ACCEPTED. FOR `f15c` ITSELF THERE IS NO CELL — see the header's last paragraph.
CONTROL_DONOR = "f15c"

# Two outcomes closer than this are the SAME outcome — on the ORDER SCALAR's own scale, where the
# smallest craft item is of order one and a level step is 1e3. Half a point is well inside the noise
# floor of the arena and far below anything that could be a level change.
kOutcomeTol = 0.5


def attr_outcome(outdir, want_key=False):
    """ONE number per run, and it is the SAME fitness the tournament ranks on: the catalogue row's
    lexicographic key, written down through fb_fitness.order_scalar — order-isomorphic to the tuple
    because C is bounded, so a BAND over it is a band over the order and not over a weighting."""
    try:
        west, east, duration = fit.read_pair(outdir, ["bandit"], ["viper"], "bandit", "viper")
    except (OSError, IndexError):
        return (None, None) if want_key else None
    key, _ = fit.side_key(west, fit.FlightView(east), duration)
    return (fit.order_scalar(key), key) if want_key else fit.order_scalar(key)


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
        control = None
        if row != CONTROL_DONOR:
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
    if row == CONTROL_DONOR:
        print("  control cell: n/a — this row IS the donor deck, so the cell would be the baseline.")
        print("                A no-op is not a measurement and no number is produced for it.")
    elif control is None:
        print("  control cell: NOT FLOWN (the run produced no telemetry)")
    else:
        print("  control cell (the row's DECK replaced by %s's, sensors/weapons/tier held): %+.1f"
              % (CONTROL_DONOR, control))
    print("  ---------------------------------------------------------------------------")
    print("  band_deck      = %10.1f    (+-10 %% on CD0/e/Ixx, +-5 %% on thrust — DIFFERENTIAL)" % band_deck)
    print("  band_doctrine  = %10.1f    (the O1 doctrine sweep — DIFFERENTIAL)" % band_doc)
    ratio = band_deck / band_doc if band_doc > 1e-9 else float("inf")
    print("  ratio          = %10.3f   (admissible iff <= %.2f)" % (ratio, threshold))

    # The control cell is a FINITE difference and only becomes commensurable through this ratio. The
    # tolerance is on the SCORE's own scale: a duel scores in the hundreds and its smallest weighted
    # term is of order one, so anything under half a point is the same outcome. Without it the ratio of
    # two floating-point zeros printed 0.6295 as if it were a measurement.
    cell_ok = True
    if control is not None and base is not None:
        disp = abs(control - base)
        share = band_deck / disp if disp > kOutcomeTol else float("inf")
        print("  |control-base| = %10.1f    (a DIFFERENT AEROPLANE — finite, not differential)" % disp)
        print("  band_deck/|control-base| = %s   our declared ignorance as a fraction of that"
              % ("n/a" if disp <= kOutcomeTol else "%.4f" % share))
        if disp <= max(band_deck, kOutcomeTol):
            cell_ok = False
            print("  INSTRUMENT DEFECT (module.md §Spec 11 instrument 3, the ONE condition it names):")
            print("           replacing the whole aeroplane moved the outcome no further than +-10 % of")
            print("           our own uncertainty did. This geometry does not discriminate decks, so")
            print("           NEITHER band means anything here and no result is printed. The arena is")
            print("           SATURATED — read the perturbation column: every point is the same outcome.")
    if band_doc > 1e-9 and ratio <= threshold and cell_ok:
        print("  VERDICT: ADMISSIBLE — the campaign is measuring the doctrine, and the result is")
        print("           %+.1f, printed WITH both bands beside it." % (base if base is not None else float("nan")))
        return 0
    if not cell_ok:
        return 1
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

    points = {v.name: 0.0 for v in variants}     # pairwise-domination points, 1 / 0.5 / 0 per SEAT
    runs = {v.name: 0 for v in variants}
    seatkey = {}                                 # (west, east) -> (west key, east key), for the match
    record = {v.name: [0, 0, 0] for v in variants}   # kills, losses, draws — a report column, not a term
    keyseen = {v.name: [] for v in variants}
    # WHICH LEVEL DECIDED, counted over every run. It is the instrument that stops a craft residue from
    # reading as a result: a field in which no run is decided at V or M has been ranked by the level
    # that was only ever meant to order ties, and doc/doctrine-evolution.md §6 says such a rank change
    # is expressly NOT a finding.
    decided = {"V": 0, "M": 0, "C": 0, "tie": 0}
    print("=" * 100)
    print("PAIRINGS — every run, both seat assignments, with the itemised key for each side")
    print("=" * 100)
    for tag, west, east in meta:
        wm, em, duration = read_run(os.path.join(args.out, tag), west.name, east.name, args.flight)
        wf, ef = fit.FlightView(wm), fit.FlightView(em)
        wkey, witems = fit.side_key(wm, ef, duration)
        ekey, eitems = fit.side_key(em, wf, duration)
        seatkey[(west.name, east.name)] = (wkey, ekey)
        runs[west.name] += 1
        runs[east.name] += 1
        keyseen[west.name].append(wkey)
        keyseen[east.name].append(ekey)
        for me, foe, who in ((wf, ef, west.name), (ef, wf, east.name)):
            if not foe.effective and me.effective:
                record[who][0] += 1
            elif not me.effective:
                record[who][1] += 1
            else:
                record[who][2] += 1
        print("\n%-28s exit=%d  %.1f sim-s   %s [%s]  vs  %s [%s]" %
              (tag, exits[tag], duration, west.name, fit.key_str(wkey), east.name, fit.key_str(ekey)))
        rows = [(m, west, items) for m, items in zip(wm, witems)] + \
               [(m, east, items) for m, items in zip(em, eitems)]
        for side, var, items in rows:
            print("  %-6s %-14s %-8s %-42s objectives %s" %
                  (side.name, var.name, side.result, side.reason[:42],
                   "/".join(side.objectives) or "-"))
            print("       detect %5s  lock %5s  shot %5s  supp %4s  threat %5s  react %5s  defend %5s  shots %d  chaff %d"
                  % (fmt(side.g("eng_detect_s")), fmt(side.g("eng_lock_s")), fmt(side.g("eng_shot_s")),
                     ("%.2f" % side.g("eng_support_f", 0.0)), fmt(side.g("eng_threat_s")),
                     fmt(side.g("eng_react_s")), ("%.1f" % side.g("eng_defend_s", 0.0)),
                     int(side.g("eng_shots", 0.0)), int(side.g("eng_chaff", 0.0))))
            for label, value, detail in items:
                print("       %+8.1f  %-14s %s" % (value, label, detail))

    # THE MATCH: the two mirrored runs of a pairing, compared SEAT AGAINST THE SAME SEAT. Comparing the
    # two sides inside one run scores the SEAT wherever the seat carries the key, and then no doctrine
    # can move the result — fb_fitness.match_points carries the measurement.
    for i in range(len(variants)):
        for j in range(i + 1, len(variants)):
            a, b = variants[i].name, variants[j].name
            (aw, be), (bw, ae) = seatkey[(a, b)], seatkey[(b, a)]
            ap, bp = fit.match_points(aw, ae, bw, be)
            points[a] += 2.0 * ap
            points[b] += 2.0 * bp
            for x, y in ((aw, bw), (ae, be)):
                decided["V" if x[0] != y[0] else "M" if x[1] != y[1]
                        else "C" if x[2] != y[2] else "tie"] += 1

    print("\n" + "=" * 100)
    print("RANKING — the fitness is a normalised WIN RATE in [0,1]: per MATCH (the two mirrored runs of")
    print("a pairing) the two variants' (V, M, C) keys are compared SEAT AGAINST THE SAME SEAT, left to")
    print("right, and the winner of a seat takes 1, a tie a half. `V` and `M` are the means of the two")
    print("levels that DECIDE; `craft` only ever orders seats that tied on both.")
    print("=" * 100)
    print("%-16s %8s %6s %6s %8s   %4s %4s %4s   %s" %
          ("variant", "fitness", "V", "M", "craft", "kill", "lost", "draw", "parameters"))
    for v in sorted(variants, key=lambda x: -points[x.name] / max(1, runs[x.name])):
        k, l, d = record[v.name]
        n = max(1, runs[v.name])
        ks = keyseen[v.name]
        gated = sum(1 for x in ks if x[2] == fit.GATE)
        craft = [x[2] for x in ks if x[2] != fit.GATE]
        print("%-16s %8.3f %6.2f %6.2f %8s   %4d %4d %4d   %s" %
              (v.name, points[v.name] / n, sum(x[0] for x in ks) / n, sum(x[1] for x in ks) / n,
               ("GATE x%d" % gated) if gated else "%+.1f" % (sum(craft) / max(1, len(craft))),
               k, l, d, " ".join("%s=%g" % kv for kv in sorted(v.params.items())) or "(airframe defaults)"))

    n_cmp = sum(decided.values())
    print("\ndecided at level:  V %d   M %d   C %d   exact tie %d   (of %d seat comparisons)"
          % (decided["V"], decided["M"], decided["C"], decided["tie"], n_cmp))
    if decided["V"] == 0 and decided["M"] == 0:
        print("SATURATED: not one seat of this field was decided by the RESULT. The order above is a")
        print("           craft residue and is expressly NOT a finding (doc/doctrine-evolution.md §6).")
        print("           Fix the arena (§4), do not read the ranking.")

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
