#!/usr/bin/env python3
"""FlightBox — THE FITNESS, on its own so that exactly one of it exists.

doc/doctrine-evolution.md §1. The order is LEXICOGRAPHIC and not weighted:

    key(unit, run) = ( V , M , C )                      larger is better

    V   the JUDGE's verdict for this unit, read from `UNIT_RESULT`, never recomputed
    M   how many of the objectives this unit DECLARED the judge marked `met`, read from the
        `mission OBJECTIVE` lines the judge publishes at every conclusion
    C   craft — TWO bounded sums, one per currency (air, aim), compared by DOMINATION

The first level that differs decides; a level is consulted only on an exact tie of every level to its
left. That is the whole point: an exchange rate between levels would be a standing offer and a search
procedure is a machine for accepting one cheaply (§1.2). Two items of the previous weighted score are
GONE rather than re-tuned — `hits landed` (a payment per event on a count the simulator partitions
into ticks, §1.1 exhibit C) and `no shot` (a price that could be bought back, replaced by a gate).

WHY C IS A PAIR AND NOT A SUM. Every craft item of the air half is an air-to-air quantity, so a strike
cell's key used to be (V, M, GATE) and two doctrines that both survived and both missed were EXACTLY
tied — [MESS, E5] on 32 of the 46 campaign cells that aim a bomb. The missing gradient is `aimErrM`,
which the judge writes on every `stores DELIVERY` line. Adding it as a SIXTH SUMMAND would have priced
one metre of aim error in shot-geometry points, i.e. exactly the standing offer §1.2 refuses one level
up. So C carries the two currencies SEPARATELY and is compared by DOMINATION: better in one and not
worse in the other wins, better in one and worse in the other is INCOMPARABLE and ties. There is no
exchange rate to accept, and a doctrine can never buy aim error with air craft or the other way round.

WHY A SCALAR EXISTS ANYWAY (`order_scalar`). Both components are bounded, so V*1e6 + M*1e3 + air + aim
is order-CONSISTENT at the two levels that decide and collapses the craft level onto a projection. On
an air-only cell `aim` is 0 in every run and the scalar is the number the attribution instrument of
doc/modules/air/module.md §Spec 11 always read. It is not a second fitness and nothing may weight
across LEVELS; the projection inside the craft level is booked as a deviation (D10).

Stdlib only. Imported by fb_tournament.py, fb_arena_check.py and fb_evolve.py.
"""

import csv
import math
import os
import re

# ---------------------------------------------------------------------------------------------
# LEVEL V — the four outcomes, and they are the judge's own (doc/missions/verdict.md).
# CRASH and LOC share rank 0 and are deliberately NOT separated: both say the same thing about the
# doctrine — it lost the jet without an opponent. TIMEOUT above FAIL: deciding nothing is worse than
# winning and better than losing.
# ---------------------------------------------------------------------------------------------
V_RANK = {"SUCCESS": 3, "TIMEOUT": 2, "NONE": 2, "FAIL": 1, "CRASH": 0, "LOC": 0}

# LEVEL C — the same items as the weighted score minus the two that paid per event. Under a
# lexicographic order the SIZES stop being load-bearing: only the signs and the boundedness matter,
# because no value of C can cross a level. That is the one property worth checking and it is checkable.
W_QUALITY = 100.0    # launch geometry: a property of the DECISION, capped by construction
W_SUPPORT = 80.0     # the difference between a launch and a kill; a fraction, so unfarmable
W_LEAD = 40.0        # relative to the opponent in the SAME run — a duel is a relative thing
W_DEFENCE = 40.0     # being shot at is the opponent's decision, so it cannot be farmed
W_ENERGY = 40.0      # the state the engagement is left in
W_SHOT_COST = 25.0   # per round fired
C_ROUNDS_FLOOR = -150.0   # bounded by the loadout, not by the run length

# THE GROUND CURRENCY — one item, and it is a MEAN of a bounded per-delivery quality, never a sum over
# deliveries: a payment per event on a count the mission author picks (how many bombs hang on the
# aircraft) is Exhibit C's defect in a second currency. kAimHalfM is the half-quality distance and it
# is NOT load-bearing: under a lexicographic order only the sign and the boundedness of a craft item
# matter, and 1/(1+e/h) is strictly decreasing everywhere, so no value of h creates or destroys an
# order between two deliveries. It is [SET] at the bottom of the measured achievable band — X-3's
# release lattice leaves a residual floor of 10-22 m on all eight strike cells it was swept on — so
# that the errors this tree can actually fly sit on the curve's steep part rather than in its tail.
W_AIM = 100.0
kAimHalfM = 10.0

C_MIN = -(abs(C_ROUNDS_FLOOR) + W_LEAD)          # -190
C_MAX = W_QUALITY + W_SUPPORT + W_LEAD + W_DEFENCE + W_ENERGY + W_AIM   # +400, still < the 1e3 step
GATE = float("-inf")     # a unit that neither fired, designated nor delivered: last in its own class


def shot_quality(s):
    """0..1 for the OPENING shot: where inside the launch zone it was taken, times how far off the
    nose the target was. Plateaued at Rtr — rewarding "closer is better" without limit would pay for
    pressing in, which level V already prices absolutely."""
    r, rtr, raero = s.g("eng_shot_nm"), s.g("eng_shot_rtr_nm"), s.g("eng_shot_raero_nm")
    if r < 0 or rtr < 0 or raero <= rtr:
        return 0.0
    zone = max(0.0, min(1.0, (raero - r) / (raero - rtr)))
    ata = abs(s.g("eng_shot_ata", 0.0))
    return zone * max(0.0, math.cos(math.radians(min(ata, 90.0))))


def aim_quality(errs):
    """0..1 over this unit's DELIVERIES: the mean of 1/(1 + e/kAimHalfM). Strictly decreasing in every
    single error, so two runs that differ in one delivery can always be ordered; bounded per delivery,
    so a fourth bomb cannot buy what the first three missed."""
    if not errs:
        return 0.0
    return sum(1.0 / (1.0 + max(0.0, e) / kAimHalfM) for e in errs) / len(errs)


def engaged(s):
    """The gate, not a price: a price can be bought back and the old -250 could be repaid by
    energy + defence + support. `eng_shot_s`/`eng_lock_s` are the two columns that survive the
    engagement, and a published `stores DELIVERY` is the ground half's own record of the same fact —
    without it every striker in the campaigns is gated and the whole ground gradient is unreachable.
    Its known blind spot is booked as doc/doctrine-evolution.md E-10."""
    return s.g("eng_shot_s") >= 0 or s.g("eng_lock_s") >= 0 or bool(s.deliveries)


def craft(me, foe, duration):
    """C for one unit in one run as (air, aim), itemised — the report prints the items because the
    point of a tournament is WHY, and that has not changed. The two currencies are accumulated apart
    and never added together; `add` takes which one an item is paid in."""
    items = []
    total = {"air": 0.0, "aim": 0.0}

    def add(label, value, detail, cur="air"):
        if abs(value) < 1e-9:
            return
        total[cur] += value
        items.append((label, value, detail))

    shots = int(me.g("eng_shots", 0.0))
    q = shot_quality(me)
    add("shot geometry", W_QUALITY * q,
        "opening shot %.1f nm (Rtr %.1f, Raero %.1f), %.0f deg off the nose -> q=%.2f" %
        (me.g("eng_shot_nm"), me.g("eng_shot_rtr_nm"), me.g("eng_shot_raero_nm"),
         abs(me.g("eng_shot_ata", 0.0)), q) if shots else "no shot")

    sup = max(0.0, min(1.0, me.g("eng_support_f", 0.0)))
    add("support", W_SUPPORT * sup, "held the uplink %.0f%% of the way to the seeker (pitbull=%d)"
        % (100 * sup, int(me.g("eng_pitbull", 0.0))))

    mine, theirs = me.g("eng_shot_s"), foe.g("eng_shot_s")
    if mine >= 0 or theirs >= 0:
        a = mine if mine >= 0 else duration
        b = theirs if theirs >= 0 else duration
        add("shot lead", W_LEAD * math.tanh((b - a) / 15.0),
            "first shot at t=%.1f vs %.1f" % (a, b) if mine >= 0 and theirs >= 0
            else ("shot while %s never did" % foe.name if mine >= 0 else "never shot, %s did" % foe.name))

    react = me.g("eng_react_s")
    if me.g("eng_threat_s") >= 0 and me.effective:
        d = max(0.0, 1.0 - react / 8.0) if react >= 0 else 0.0
        add("defence", W_DEFENCE * d, "answered the warning in %.1f s and lived" % react if react >= 0
            else "never answered the warning")

    if me.es_start > 1.0:
        frac = max(0.0, min(1.0, me.g("eng_es_min", 0.0) / me.es_start))
        add("energy", W_ENERGY * frac, "energy floor %.0f%% of entry (%.0f -> %.0f ft)"
            % (100 * frac, me.es_start, me.g("eng_es_min", 0.0)))

    add("rounds", max(C_ROUNDS_FLOOR, -W_SHOT_COST * shots), "%d round(s) fired" % shots)

    q = aim_quality(me.deliveries)
    add("aim", W_AIM * q, "%d delivery(ies), mean aim error %.1f m (best %.1f) -> q=%.2f"
        % (len(me.deliveries), sum(me.deliveries) / len(me.deliveries), min(me.deliveries), q)
        if me.deliveries else "no delivery", "aim")
    return (total["air"], total["aim"]), items


def unit_key(me, foe, duration):
    """(V, M, C) for one unit against one opponent VIEW in one run."""
    v = V_RANK.get(me.result, 2)
    m = sum(1 for st in me.objectives if st == "met")
    c, items = craft(me, foe, duration)
    return (v, m, c if engaged(me) else GATE), items


def side_key(members, foe, duration):
    """A FLIGHT's key is the componentwise SUM over its members — over both craft currencies too, each
    within its own. V and M stay integers, so ties still happen naturally at those levels, and with one
    member it IS the per-unit key. The engagement gate is evaluated at SIDE level — a flight in which
    one member deliberately held its trigger under the cover rule has still engaged."""
    keys = [unit_key(m, foe, duration) for m in members]
    v = sum(k[0][0] for k in keys)
    mm = sum(k[0][1] for k in keys)
    c = tuple(sum(0.0 if k[0][2] == GATE else k[0][2][i] for k in keys) for i in (0, 1))
    if not any(engaged(m) for m in members):
        c = GATE
    return (v, mm, c), [k[1] for k in keys]


def order_scalar(key):
    """The tuple as ONE number: V and M keep their 1e6/1e3 steps and the craft level is PROJECTED onto
    the sum of its two currencies, which is bounded by 400. Order-consistent wherever V or M decides;
    inside the craft level it is a projection and not the order (a projection has an exchange rate, the
    order has none), so it is for instruments that need a scalar — the attribution bands — and never
    for a ranking. On an air-only cell `aim` is 0 and it is the number it always was."""
    v, m, c = key
    if c == GATE:
        return v * 1e6 + m * 1e3 - 1e5
    return v * 1e6 + m * 1e3 + c[0] + c[1]


def compare_craft(x, y):
    """The craft level, and the whole reason it is a pair: DOMINATION between the currencies. Better in
    one and not worse in the other wins; better in one and worse in the other is INCOMPARABLE and ties,
    because saying which of the two is worth more is exactly the exchange rate §1.2 refuses. GATE (the
    unit did not engage at all) is below every craft value and equal to itself."""
    if x == GATE or y == GATE:
        return 0 if x == GATE and y == GATE else (-1 if x == GATE else 1)
    ge = all(u >= v for u, v in zip(x, y))
    le = all(u <= v for u, v in zip(x, y))
    if ge and le:
        return 0
    return 1 if ge else -1 if le else 0


def compare(a, b):
    """-1 / 0 / +1, left to right: the first LEVEL that differs decides, and craft never crosses one."""
    for x, y in zip(a[:2], b[:2]):
        if x < y:
            return -1
        if x > y:
            return 1
    return compare_craft(a[2], b[2])


def pair_points(a, b):
    """§1.4 design C: per pairing the two units' keys are compared DIRECTLY and the variant scores
    1 / 0.5 / 0. Ties happen naturally because ranks are integers — which is exactly what a mean over
    runs destroys, and why the mean is refused."""
    r = compare(a, b)
    return (1.0, 0.0) if r > 0 else (0.0, 1.0) if r < 0 else (0.5, 0.5)


def match_points(a_west, a_east, b_west, b_east):
    """A MATCH is the PAIR of mirrored runs, and the comparison inside it is SEAT AGAINST THE SAME
    SEAT — A as west against B as west, A as east against B as east — summed and halved.

    Comparing the two sides INSIDE one run measures the SEAT wherever the seat carries the key, and
    then no genome can move the result: [MESS, `split --flight 2`, 12 pairings] the west key is
    C = +505.5…+526.7 and the east key C = +69.0 in 12 of 12, so the cross-seat comparison returns
    `west` in both mirrored runs of every pairing, every variant takes exactly one point of two, and
    the whole population scores 0.500 whatever it carries. The seat is worth 457 craft points there
    and the four genomes 21.2 — like against like keeps the 21.2 and drops the 457.

    `a_west` is A's key in the run it flew west (B east); `a_east` is A's key in the mirrored run."""
    aw, bw = pair_points(a_west, b_west)
    ae, be = pair_points(a_east, b_east)
    return (aw + ae) / 2.0, (bw + be) / 2.0


def outcome_class(key):
    """The (V, M) pair — what "the same outcome" means, and the unit the saturation criterion of §4.2
    counts modal shares in."""
    return (key[0], key[1])


# ---------------------------------------------------------------------------------------------
# Reading a run back — one unit's debrief out of its own telemetry plus the runner's verdict lines.
# ---------------------------------------------------------------------------------------------
class Side:
    def __init__(self, name):
        self.name = name
        self.result = "NONE"
        self.reason = ""
        self.effective = True
        self.hits = 0
        self.eng = {}
        self.es_start = 0.0
        self.objectives = []      # the judge's own per-objective states, in declaration order
        self.deliveries = []      # aimErrM per `stores DELIVERY` this unit's stores produced

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
    st, es = idx.get("eng_state"), idx.get("eng_es")
    if st is not None and es is not None:
        s.es_start = float(rows[1][es]) if len(rows) > 1 else 0.0
        for r in rows[1:]:
            if r[st] != "idle":
                s.es_start = float(r[es])
                break
    return s


RESULT_RE = re.compile(r'UNIT_RESULT unit=(\S+) result=(\S+) reason="([^"]*)"')
OBJECTIVE_RE = re.compile(r'mission OBJECTIVE unit=(\S+) kind=(?:"[^"]*"|\S+) state=(\S+)')
DELIVERY_RE = re.compile(r'stores DELIVERY unit=(\S+) .*\baimErrM=([-0-9.e+]+)')


def read_events(path, by_call):
    """Fills `result`/`reason`, the per-objective vector and the delivery errors, and returns the run
    duration. The OBJECTIVE lines are level M's ONLY input (doc/doctrine-evolution.md E-1); since the
    runner asks every open judge before it reports (X-1), a run that ENDS publishes them, and a unit
    with none declared none.

    A DELIVERY is attributed to the CARRIER: the line is written in the store's own log scope and a
    store is named `<carrier>_<key>_<n>`, so the two trailing fields come off."""
    duration = 0.0
    for line in open(path):
        m = RESULT_RE.search(line)
        if m and m.group(1) in by_call:
            by_call[m.group(1)].result = m.group(2)
            by_call[m.group(1)].reason = m.group(3)
            by_call[m.group(1)].name = m.group(1)
        o = OBJECTIVE_RE.search(line)
        if o and o.group(1) in by_call:
            by_call[o.group(1)].objectives.append(o.group(2))
        d = DELIVERY_RE.search(line)
        if d:
            carrier = d.group(1).rsplit("_", 2)[0]
            if carrier in by_call:
                by_call[carrier].deliveries.append(float(d.group(2)))
        if "mission SUMMARY" in line:
            d = re.search(r"durationS=([0-9.]+)", line)
            if d:
                duration = float(d.group(1))
    return duration


class FlightView:
    """The opposing FLIGHT as one side's craft terms need to see it: capable while ANY member is, and
    the first shot is the earliest of them. `hits` is kept for the report only — it pays nothing."""

    deliveries = ()

    def __init__(self, members):
        self.name = members[0].name if len(members) == 1 else "%s x%d" % (members[0].name, len(members))
        self.effective = any(m.effective for m in members)
        self.hits = sum(m.hits for m in members)
        shots = [m.g("eng_shot_s") for m in members if m.g("eng_shot_s") >= 0]
        self.eng = {"eng_shot_s": min(shots) if shots else -1.0}

    def g(self, key, default=-1.0):
        v = self.eng.get(key, default)
        return default if v is None else v


def key_str(key):
    v, m, c = key
    return "V=%d M=%d C=%s" % (v, m, "GATE" if c == GATE else "%+.1f/%+.1f" % c)


def read_pair(outdir, west_calls, east_calls, west_name, east_name):
    """Both sides of one run: the telemetry files in actor order plus the events log."""
    def path_of(call, i):
        return os.path.join(outdir, "telemetry.csv" if i == 0 else "telemetry_%s.csv" % call)
    west = [read_side(path_of(c, i), west_name) for i, c in enumerate(west_calls)]
    east = [read_side(path_of(c, i + len(west_calls)), east_name) for i, c in enumerate(east_calls)]
    by_call = dict(zip(west_calls, west))
    by_call.update(zip(east_calls, east))
    duration = read_events(os.path.join(outdir, "events.log"), by_call)
    return west, east, duration
