"""THE JUNCTION, as a place rather than as a crossing of two ribbons.

A network of ribbons is not a street network. What a driver and a walker actually read at a
junction is none of the geometry the road bed already solves -- it is the MARKINGS and the
FURNITURE, and every one of them follows from something the network already carries or a surveyor
already tagged:

    HALTELINIE     a stop bar across a minor leg at its cut. RMS-1: 0.50 m wide, and it stands
                   where the leg meets the junction's own surface, which the bed already computes
    WARTELINIE     the give-way line -- a row of triangles, RMS-1's "Haifischzaehne", where the
                   priority is `give_way` rather than `stop`
    ZEBRASTREIFEN  a crossing: RMS-1 gives 0.50 m bars with 0.50 m gaps over a 4.00 m width, and
                   it goes where OSM says `highway=crossing` or where a footway meets a carriageway
    STRASSENGULLY  a gully grating in the gutter, at the LOW point of each channel, because that
                   is where the water goes -- and the road bed already knows the profile
    LICHTSIGNAL    a signal head on a pole where OSM says `highway=traffic_signals`
    VERKEHRSZEICHEN a plate on a post where it says `traffic_sign`

None of it is invented and none of it is placed by hand. What makes a junction read is that the
markings AGREE with the priority the network already states -- a stop bar on the through road is
the tell that a generator did not look.
"""
import math

import numpy as np

STOP_M = 0.50             # [SET] RMS-1: the Haltelinie is 500 mm wide
GIVE_M = 0.50             # [SET] StVO 342: a Haifischzahn is 500 mm at the base and 500 mm long
GIVE_GAP_M = 0.25         # [SET] StVO 342: and they stand 250 mm apart
ZEBRA_BAR_M = 0.50        # [SET] RMS-1: a crossing's bars and its gaps are both 500 mm
ZEBRA_WIDE_M = 4.00       # [SET] the crossing's own width across the carriageway
GULLY_M = 0.50            # [SET] DIN 4052: a road gully's grating is 500 x 500 mm
SIGNAL_H_M = 3.00         # [SET] StVO: a signal head's lower edge over the carriageway
SIGN_H_M = 2.20           # [SET] StVO: a sign's lower edge over a footway


def _at(line, s):
    p = line.interpolate(max(0.0, min(line.length, s)))
    a = line.interpolate(max(0.0, s - 0.5))
    b = line.interpolate(min(line.length, s + 0.5))
    dx, dy = b.x - a.x, b.y - a.y
    d = math.hypot(dx, dy) or 1.0
    return (p.x, p.y), (dx / d, dy / d)


def _bar(centre, along, half_w, half_l, z_at, rise):
    """A painted bar: `half_w` across the carriageway, `half_l` along it, ON the carriageway.

    `z_at` is read at each CORNER. Read once at the bar's centre, a zebra on a crowned 10 m road
    stood 0.12 m proud at its outer end and Cycles drew four rows of kerbstones."""
    nx, ny = -along[1], along[0]
    v = []
    for (a, b) in ((-half_w, -half_l), (half_w, -half_l), (half_w, half_l), (-half_w, half_l)):
        x = centre[0] + nx * a + along[0] * b
        y = centre[1] + ny * a + along[1] * b
        v.append((x, y, z_at(x, y) + rise))
    return v, [(0, 1, 2), (0, 2, 3)]


def stop_lines(m, st, z_at, paint_m):
    """A STOP BAR ON EVERY MINOR LEG, and none on the through road. The priority is already in
    the network -- `priority` per way, which is what netconvert's own type table sets -- so a bar
    that lands on the major is a bar that says the generator did not read it."""
    out = []
    for nid, junction in m.junctions.items():
        major = junction["major"]
        for (w, k, sgn) in junction["legs"]:
            if w["id"] == major:
                continue
            cut = st.cuts.get((w["id"], nid))
            if cut is None:
                continue
            s = m.stations[w["id"]][k]
            here = s + cut * (1.0 if sgn > 0 else -1.0)
            line = m.centreline(w)
            (x, y), d = _at(line, here)
            half = w["tags"]["width"] / 2.0
            give = str(w["tags"].get("priority", 5)) and float(w["tags"].get("priority", 5)) >= 4
            if give:
                # the WARTELINIE: a row of triangles, base to the driver
                n = max(3, int(2 * half / (GIVE_M + GIVE_GAP_M)))
                for i in range(n):
                    off = -half + (i + 0.5) * (2 * half / n)
                    nx, ny = -d[1], d[0]
                    tip = (x + nx * off + d[0] * GIVE_M, y + ny * off + d[1] * GIVE_M)
                    a = (x + nx * (off - GIVE_M / 2), y + ny * (off - GIVE_M / 2))
                    b = (x + nx * (off + GIVE_M / 2), y + ny * (off + GIVE_M / 2))
                    out.append(("paint", [(q[0], q[1], z_at(*q) + paint_m) for q in (a, b, tip)],
                                [(0, 1, 2)]))
            else:
                out.append(("paint", *_bar((x, y), d, half * 0.98, STOP_M / 2, z_at, paint_m)))
    return tuple(out)


def crossing_sites(m, st):
    """WHERE A CROSSING GOES, as (centre, direction along the road, half width across).

    One source: the paint reads it and so does the kerb, because a crossing without a dropped
    kerb is a step a wheelchair cannot take and a detail every one of the references draws."""
    want = set()
    for nid, junction in m.junctions.items():
        for (w, k, sgn) in junction["legs"]:
            if w["tags"].get("highway") in ("motorway", "motorway_link", "trunk", "footway",
                                            "path", "steps", "cycleway", "service", "track"):
                continue
            cut = st.cuts.get((w["id"], nid), 0.0)
            want.add((w["id"], k, sgn, cut + 2.6))
    out = []
    for (wid, k, sgn, back) in sorted(want):
        w = m.way_by_id(wid)
        line = m.centreline(w)
        s = m.stations[wid][k] + back * (1.0 if sgn > 0 else -1.0)
        if s < 0.5 or s > line.length - 0.5:
            continue
        (x, y), d = _at(line, s)
        out.append(((x, y), d, w["tags"]["width"] / 2.0))
    return tuple(out)


def crossings(m, st, z_at, paint_m, nodes=None):
    """A ZEBRA where OSM says one, and at every junction leg that carries a footway beside it.

    RMS-1: bars 500 mm wide with 500 mm gaps, across a 4.00 m crossing, set back from the junction
    by the stop bar's own room. A crossing drawn without that setback lands inside the junction
    surface, which is where no crossing has ever been painted."""
    out = []
    for ((x, y), d, half) in crossing_sites(m, st):
        n = max(2, int(2 * half / (2 * ZEBRA_BAR_M)))
        for i in range(n):
            off = -half + ZEBRA_BAR_M * 0.5 + i * 2 * ZEBRA_BAR_M
            if abs(off) > half - ZEBRA_BAR_M * 0.4:
                continue
            nx, ny = -d[1], d[0]
            out.append(("paint", *_bar((x + nx * off, y + ny * off), d,
                                       ZEBRA_BAR_M / 2, ZEBRA_WIDE_M / 2, z_at, paint_m)))
    return tuple(out)


def gullies(m, st, z_at, kerb_up, gutter_off):
    """A GULLY WHERE THE WATER GOES: the low point of each channel. The road bed already solves
    the profile, so the low points are read off it and not guessed -- a gully on a crest is the
    tell that nobody looked at the grade."""
    out = []
    for w in m.net.ways:
        if w["tags"].get("highway") in ("motorway", "footway", "path", "steps", "cycleway"):
            continue
        line = m.centreline(w)
        if line.length < 12.0:
            continue
        ss = np.linspace(0.0, line.length, max(8, int(line.length / 5.0)))
        g = np.array([m.profile(w, float(v))[1] for v in ss])
        # a LOW POINT is where the grade turns from falling to rising
        for i in range(1, len(ss) - 1):
            if g[i - 1] < 0.0 <= g[i + 1]:
                (x, y), d = _at(line, float(ss[i]))
                half = w["tags"]["width"] / 2.0 + gutter_off
                for side in (-1.0, 1.0):
                    nx, ny = -d[1] * side, d[0] * side
                    out.append(("iron", *_bar((x + nx * half, y + ny * half), d,
                                              GULLY_M / 2, GULLY_M / 2, z_at, 0.004)))
    return tuple(out)


def _post(x, y, z, height, radius, sides=8):
    verts, tris = [], []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        verts += [(x + radius * math.cos(a), y + radius * math.sin(a), z),
                  (x + radius * math.cos(a), y + radius * math.sin(a), z + height)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


def _plate(x, y, z, facing, w, h, thick=0.03):
    nx, ny = -facing[1], facing[0]
    v = []
    for (a, b, c) in ((-w, 0, 0), (w, 0, 0), (w, 0, h), (-w, 0, h),
                      (-w, thick, 0), (w, thick, 0), (w, thick, h), (-w, thick, h)):
        v.append((x + nx * a + facing[0] * b, y + ny * a + facing[1] * b, z + c))
    t = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
         (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return v, t


def signals_and_signs(doc, frame, z_at):
    """WHERE OSM SAYS ONE STANDS. A signal head on a pole at `highway=traffic_signals`, a plate
    at `traffic_sign`, and a stop plate at `highway=stop` -- the surveyor put them there."""
    nodes = {e["id"]: e for e in doc["elements"] if e["type"] == "node"}
    out = []
    for e in nodes.values():
        tags = e.get("tags") or {}
        if not tags:
            continue
        x, y = frame.xy(e["lat"], e["lon"])
        if abs(x) > 3000.0 or abs(y) > 3000.0:
            continue
        z = z_at(x, y)
        if tags.get("highway") == "traffic_signals":
            out.append(("iron", *_post(x, y, z, SIGNAL_H_M + 0.9, 0.06)))
            out.append(("iron", *_plate(x, y, z + SIGNAL_H_M, (1.0, 0.0), 0.16, 0.90, 0.16)))
        elif "traffic_sign" in tags or tags.get("highway") in ("stop", "give_way"):
            out.append(("iron", *_post(x, y, z, SIGN_H_M + 0.7, 0.032)))
            out.append(("paint", *_plate(x, y, z + SIGN_H_M, (1.0, 0.0), 0.32, 0.64)))
    return tuple(out)
