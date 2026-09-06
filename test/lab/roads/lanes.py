"""A CARRIAGEWAY IS A LIST OF LANES, and everything painted on it follows from that list.

Drawn as ONE ribbon with a line down the middle, a 10 m `primary` has two 4.75 m lanes -- which
no road on this planet has, and which reads as an airstrip with a stripe (board:2158, seen in the
first plan of a T-junction). SUMO's `netconvert` is the readable baseline CLAUDE.md names for
anything a road IS: it derives a lane count from `osmNetconvert.typ.xml` per `highway=*` and lets
OSM's own `lanes*` tags override it, and CARLA's `MeshFactory` then samples EACH lane. The same
order here, with German numbers because the classes are German:

    the SURVEYOR decides where he wrote it down -- `lanes`, `lanes:forward`, `lanes:backward`,
    `oneway`, `parking:lane:*`. That is data and it wins
    the CLASS decides where he did not: RASt 06 Tabelle 26 gives the Fahrstreifenbreite per
    class, the width the bed already carries is divided by it, and the count falls out
    the RESULT is bounded: no lane narrower than 2.25 m and none wider than 4.00 m, because a
    lane outside that band is not a lane but a carriageway nobody divided. WHAT IS LEFT OVER IS
    NOT POURED INTO THE LANES -- a 9.5 m Bundesstrasse in a town is two 3.5 m lanes and a parking
    lane, never two 4.75 m ones, and that is the whole reason a class carries a lane CAP

A lane's `use` is what stands on it -- `drive`, `park`, `cycle`, `bus` -- and R7's parked car and
R5's cycle lane both read that word rather than inventing a place to stand.
"""

# RASt 06 Tabelle 26: the Fahrstreifenbreite a class is designed with, in metres
LANE_M = {"motorway": 3.75, "motorway_link": 3.75, "trunk": 3.75, "trunk_link": 3.75,
          "primary": 3.50, "primary_link": 3.25, "secondary": 3.25, "secondary_link": 3.25,
          "tertiary": 3.25, "tertiary_link": 3.00, "unclassified": 3.00, "residential": 3.00,
          "living_street": 2.75, "service": 2.75, "pedestrian": 3.00, "road": 3.00}
PARK_M = 2.00             # [SET] RASt 06: a longitudinal parking lane, kerb to the driving lane
CYCLE_M = 1.60            # [SET] RASt 06: a Radfahrstreifen, its own line included
EDGE_M = 0.25             # [SET] the margin between the outer lane and the gutter
LANE_MIN_M, LANE_MAX_M = 2.25, 3.75   # [SET] RASt 06's band: 3.75 is the widest, for a lorry route
# HOW MANY DRIVING LANES A CLASS CARRIES PER DIRECTION at most. Without it every metre of width
# a way happens to carry becomes another lane, and a Bundesstrasse with parking on both sides
# reads as a four-lane road with none.
CAP = {"motorway": 4, "motorway_link": 2, "trunk": 4, "trunk_link": 2,
       "primary": 2, "primary_link": 1, "secondary": 2, "secondary_link": 1,
       "tertiary": 1, "tertiary_link": 1, "unclassified": 1, "residential": 1,
       "living_street": 1, "service": 1, "pedestrian": 1, "road": 1}
NO_LANES = ("footway", "path", "steps", "cycleway", "track", "bridleway")
# RMS-1: a Leitlinie is one part stroke to two parts gap. The two standard lengths are 6/12
# outside a built-up area and 3/6 inside one, and the CLASS is what says which applies here.
DASH_FAST = (6.0, 12.0)
DASH_TOWN = (3.0, 6.0)
FAST = ("motorway", "motorway_link", "trunk", "trunk_link")


class Lane:
    """One lane: where its CENTRE is across the way, how wide, which way it runs, what it is."""

    __slots__ = ("centre", "width", "forward", "use")

    def __init__(self, centre, width, forward, use):
        self.centre = float(centre)
        self.width = float(width)
        self.forward = bool(forward)
        self.use = str(use)

    @property
    def left(self):
        return self.centre + self.width / 2.0

    @property
    def right(self):
        return self.centre - self.width / 2.0

    def __repr__(self):
        way = "fwd" if self.forward else "bwd"
        return f"Lane({self.centre:+.2f} w{self.width:.2f} {way} {self.use})"


def _int(tags, key):
    raw = str(tags.get(key, "")).split(";")[0].strip()
    try:
        got = int(float(raw))
    except ValueError:
        return 0
    return got if got > 0 else 0


def is_oneway(tags):
    return str(tags.get("oneway", "")).lower() in ("yes", "1", "true", "-1")


def dash(tags):
    """RMS-1's Leitlinie, as (stroke, gap) in metres."""
    return DASH_FAST if tags.get("highway") in FAST else DASH_TOWN


def counts(tags, width):
    """How many lanes each way, from the tags where they exist and from the width where they do
    not. Returns (forward, backward)."""
    one = is_oneway(tags)
    fwd, bwd = _int(tags, "lanes:forward"), _int(tags, "lanes:backward")
    told = _int(tags, "lanes")
    if fwd or bwd:
        if told and not (fwd and bwd):
            fwd = fwd or max(0, told - bwd)
            bwd = bwd or max(0, told - fwd)
        return max(1, fwd), (0 if one else max(0, bwd))
    if told:
        return (told, 0) if one else (max(1, told // 2), max(1, told - told // 2))
    kind = tags.get("highway") or ""
    lane = LANE_M.get(kind, 3.00)
    cap = CAP.get(kind, 1)
    usable = max(0.0, width - 2 * EDGE_M)
    per = usable if one else usable / 2.0
    n = max(1, min(cap, int(round(per / lane))))
    # the band is the correction as well as the check: a lane under the minimum means one too many
    while n > 1 and per / n < LANE_MIN_M:
        n -= 1
    if not one and per < LANE_MIN_M:
        # A ROAD TOO NARROW FOR TWO LANES HAS ONE, and both directions share it. A 4 m service
        # road is einspurig -- two cars do not pass on it, it carries no centre line, and
        # dividing it into two 1.75 m lanes states something no marking on it ever said.
        return (1, 0)
    return (n, 0) if one else (n, n)


def parking(tags):
    """Which sides carry a parking lane, as (left, right). RASt 06 puts it against the kerb."""
    kinds = ("parallel", "diagonal", "perpendicular", "marked", "on_street")
    side = str(tags.get("parking:lane:both", "")).lower()
    left = str(tags.get("parking:lane:left", side)).lower()
    right = str(tags.get("parking:lane:right", side)).lower()
    return (any(k in left for k in kinds), any(k in right for k in kinds))


def cycle(tags):
    """Which sides carry a Radfahrstreifen, as (left, right)."""
    both = str(tags.get("cycleway", "")).lower() in ("lane", "track")
    left = both or str(tags.get("cycleway:left", "")).lower() in ("lane", "track")
    right = both or str(tags.get("cycleway:right", "")).lower() in ("lane", "track")
    return (left, right)


def of(way):
    """THE LANES OF A WAY, left to right across it -- offsets in the way's own frame, so a
    positive offset is to the LEFT of the direction the refs run."""
    tags = way["tags"]
    if tags.get("highway") in NO_LANES or tags.get("railway") or "railway" in tags:
        return ()
    width = float(tags.get("width") or 0.0)
    if width <= 0.0:
        return ()
    fwd, bwd = counts(tags, width)
    n = fwd + bwd
    if not n:
        return ()
    usable = width - 2 * EDGE_M
    lane = LANE_M.get(tags.get("highway"), 3.00)
    park_l, park_r = parking(tags)
    cyc_l, cyc_r = cycle(tags)
    # WHAT IS LEFT OVER AFTER THE LANES BELONGS TO SOMETHING, and a fat lane is the one thing it
    # never belongs to: a 9.5 m Bundesstrasse in a town is two 3.5 m lanes and a parking lane,
    # never two 4.75 m ones. A driving lane is therefore capped at its CLASS's width and the
    # remainder is offered to parking, then to a cycle lane, and only what no one takes widens
    # the margin at the kerb.
    def spare(extras):
        return usable - n * lane - sum(w for (_, w, _) in extras)

    extras = []
    if park_l:
        extras.append(("L", PARK_M, "park"))
    if park_r:
        extras.append(("R", PARK_M, "park"))
    if cyc_l:
        extras.append(("L", CYCLE_M, "cycle"))
    if cyc_r:
        extras.append(("R", CYCLE_M, "cycle"))
    # A SURVEYOR'S TAG IS DROPPED ONE SIDE AT A TIME, never both at once: a street too narrow for
    # parking on both sides still parks on one, which is what the narrow ones actually do.
    order = ["cycle-L", "cycle-R", "park-L", "park-R"]
    while extras and (usable - sum(w for (_, w, _) in extras)) / n < LANE_MIN_M:
        for tag in order:
            use, side = tag.split("-")
            hit = next((e for e in extras if e[0] == side and e[2] == use), None)
            if hit:
                extras.remove(hit)
                break
        else:
            break
    # A PARKING LANE IS ADDED IN PAIRS OR NOT AT ALL. One side is a thing a SURVEYOR states;
    # invented on one side it moves the centre line off the axis of a symmetric street, which
    # is a claim about the street that nobody made (rendered in plan and looked at).
    if not any(e[2] == "park" for e in extras) and spare(extras) >= 2 * PARK_M:
        extras.append(("L", PARK_M, "park"))
        extras.append(("R", PARK_M, "park"))
    drive = min(lane, (usable - sum(w for (_, w, _) in extras)) / n)
    # laid out from the LEFT edge inward: left parking, left cycle, backward lanes, forward
    # lanes, right cycle, right parking -- the order a German street has them in
    taken = n * drive + sum(w for (_, w, _) in extras)
    out = []
    at = taken / 2.0
    for use in ("park", "cycle"):
        for e in [e for e in extras if e[0] == "L" and e[2] == use]:
            out.append(Lane(at - e[1] / 2.0, e[1], False, use))
            at -= e[1]
    for _ in range(bwd):
        out.append(Lane(at - drive / 2.0, drive, False, "drive"))
        at -= drive
    for _ in range(fwd):
        out.append(Lane(at - drive / 2.0, drive, True, "drive"))
        at -= drive
    for use in ("cycle", "park"):
        for e in [e for e in extras if e[0] == "R" and e[2] == use]:
            out.append(Lane(at - e[1] / 2.0, e[1], True, use))
            at -= e[1]
    return tuple(out)


def boundaries(way):
    """WHERE A LINE GOES AND WHICH ONE, between one lane and the next.

    Returns [(offset, kind)] with kind in `lead` (Leitlinie, dashed), `divide` (a
    Fahrstreifenbegrenzung, solid, between a driving lane and one that is not driven) and
    `edge` (the Fahrbahnbegrenzung at the outside)."""
    got = of(way)
    if len(got) < 1:
        return ()
    out = []
    for a, b in zip(got, got[1:]):
        at = (a.right + b.left) / 2.0
        if a.use != "drive" or b.use != "drive":
            out.append((at, "divide"))
        else:
            out.append((at, "lead"))
    if way["tags"].get("highway") in ("primary", "secondary", "trunk", "motorway",
                                      "primary_link", "secondary_link"):
        out.append((got[0].left, "edge"))
        out.append((got[-1].right, "edge"))
    return tuple(out)


def check_lane_band(ways):
    """I20: NO LANE OUTSIDE RASt 06's OWN BAND. The worst lane width over a network, and the
    count of lanes outside 2.25 to 4.00 m.

    The control is the construction this replaces: one carriageway with a line down the middle,
    which on every synthetic major reads 4.75 m."""
    widest, bad, n, over = 0.0, 0, 0, 0.0
    for w in ways:
        got = of(w)
        if not got:
            continue
        for lane in got:
            if lane.use != "drive":
                continue
            n += 1
            widest = max(widest, lane.width)
            if lane.width < LANE_MIN_M - 1e-6 or lane.width > LANE_MAX_M + 1e-6:
                bad += 1
        # nothing may be laid outside the carriageway the bed already solved
        half = float(w["tags"]["width"]) / 2.0
        over = max(over, max(got[0].left, -got[-1].right) - half)
    return {"lanes": n, "outside band": bad, "widest m": widest, "outside carriageway m": over}


def control_two_lanes(ways):
    """THE NEGATIVE CONTROL FOR I20: the construction this file replaces -- one carriageway with
    a line down the middle, so two lanes whatever the class and whatever the width."""
    widest, bad, n = 0.0, 0, 0
    for w in ways:
        if w["tags"].get("highway") in NO_LANES or w["tags"].get("railway"):
            continue
        width = float(w["tags"].get("width") or 0.0)
        if width <= 0.0:
            continue
        half = (width - 2 * EDGE_M) / 2.0
        for _ in range(2):
            n += 1
            widest = max(widest, half)
            if half < LANE_MIN_M - 1e-6 or half > LANE_MAX_M + 1e-6:
                bad += 1
    return {"lanes": n, "outside band": bad, "widest m": widest}
