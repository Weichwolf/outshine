"""WHAT A SURVEYOR ALREADY PUT ON THE PAVEMENT, and it is a hundred thousand things a level
designer would otherwise place by hand.

OSM carries them as NODES with a tag: a bench, a bin, a bus stop with its shelter, a post box, a
drinking fountain, a bicycle stand, a phone box, a bollard, a gate. Every one of them is street
furniture in the sense the references mean it -- what a player passes within arm's reach -- and
none of it is invention: the position is surveyed and the DIMENSIONS come from the standard the
thing is built to.

    BANK        DIN EN 1176 / RASt 06: a public bench is 1.80 m long, the seat 0.45 m up, the
                back 0.85 m
    ABFALLBEHAELTER  a street bin: 0.40 m across, 0.75 m tall, on a post
    WARTEHALLE  a bus shelter: RASt 06 gives 1.50 m of depth and 4.00 m of length at a stop, the
                roof 2.50 m up
    HALTESTELLENMAST  the flag on its post, 3.00 m -- what a player actually looks for
    BRIEFKASTEN 0.50 x 0.35 x 0.60 on a post, 1.10 m to its base
    POLLER      DIN 79008: 0.90 m over the ground, 0.11 m across

Each is one function and one line in the registry, so a variant is a function beside it. The
ORIENTATION is the one thing OSM does not carry, so it is taken from the street the thing stands
on -- a bench facing away from the carriageway is the tell that nobody looked.
"""
import math

import numpy as np

SEAT_M = 0.45             # [SET] RASt 06: the seat height of a public bench
BACK_M = 0.85             # [SET] and its back
BENCH_M = 1.80            # [SET] its length
BIN_M = 0.40              # [SET] a street bin's diameter
BIN_H_M = 0.75
SHELTER_L_M = 4.00        # [SET] RASt 06: a bus shelter at a stop
SHELTER_D_M = 1.50
SHELTER_H_M = 2.50
FLAG_H_M = 3.00           # [SET] the stop's own mast
POST_M = 1.10

MADE = {}


def makes(*tags):
    def keep(fn):
        for t in tags:
            MADE[t] = fn
        return fn
    return keep


def _box(o, along, out, length, depth, lo, hi):
    """A box in the frame the street gives: `along` down the kerb, `out` away from it."""
    up = (0.0, 0.0, 1.0)
    v = []
    for (a, b, c) in ((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0),
                      (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)):
        v.append((o[0] + along[0] * a * length / 2 + out[0] * b * depth / 2,
                  o[1] + along[1] * a * length / 2 + out[1] * b * depth / 2,
                  o[2] + (hi if c else lo)))
    t = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
         (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return v, t


def _post(o, height, radius, sides=8):
    verts, tris = [], []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        verts += [(o[0] + radius * math.cos(a), o[1] + radius * math.sin(a), o[2]),
                  (o[0] + radius * math.cos(a), o[1] + radius * math.sin(a), o[2] + height)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


@makes("bench")
def bench(o, along, out):
    """A seat and a back, and the back is on the side AWAY from the carriageway."""
    got = [("timber", *_box(o, along, out, BENCH_M, 0.45, SEAT_M - 0.05, SEAT_M))]
    back = (o[0] + out[0] * 0.20, o[1] + out[1] * 0.20, o[2])
    got.append(("timber", *_box(back, along, out, BENCH_M, 0.06, SEAT_M, BACK_M)))
    for side in (-1.0, 1.0):
        leg = (o[0] + along[0] * side * BENCH_M * 0.40, o[1] + along[1] * side * BENCH_M * 0.40, o[2])
        got.append(("iron", *_post(leg, SEAT_M, 0.03, 6)))
    return got


@makes("waste_basket")
def bin_(o, along, out):
    return [("iron", *_post(o, POST_M * 0.6, 0.035, 6)),
            ("iron", *_post((o[0], o[1], o[2] + POST_M * 0.55), BIN_H_M, BIN_M / 2, 12))]


@makes("post_box")
def post_box(o, along, out):
    return [("iron", *_post(o, POST_M, 0.05, 8)),
            ("paint", *_box((o[0], o[1], o[2]), along, out, 0.50, 0.35, POST_M, POST_M + 0.60))]


@makes("bus_stop", "shelter")
def shelter(o, along, out):
    """A WARTEHALLE and its mast: a roof on four posts with a back wall of glass."""
    got = [("iron", *_post((o[0] + along[0] * 2.2, o[1] + along[1] * 2.2, o[2]), FLAG_H_M, 0.05, 8))]
    for a in (-1.0, 1.0):
        for b in (-1.0, 1.0):
            leg = (o[0] + along[0] * a * SHELTER_L_M / 2 + out[0] * b * SHELTER_D_M / 2,
                   o[1] + along[1] * a * SHELTER_L_M / 2 + out[1] * b * SHELTER_D_M / 2, o[2])
            got.append(("steel", *_post(leg, SHELTER_H_M, 0.045, 6)))
    got.append(("steel", *_box(o, along, out, SHELTER_L_M + 0.2, SHELTER_D_M + 0.2,
                               SHELTER_H_M, SHELTER_H_M + 0.10)))
    back = (o[0] + out[0] * SHELTER_D_M / 2, o[1] + out[1] * SHELTER_D_M / 2, o[2])
    got.append(("glass", *_box(back, along, out, SHELTER_L_M, 0.03, 0.30, SHELTER_H_M - 0.15)))
    seat = (o[0] + out[0] * (SHELTER_D_M / 2 - 0.30), o[1] + out[1] * (SHELTER_D_M / 2 - 0.30), o[2])
    got.append(("timber", *_box(seat, along, out, SHELTER_L_M - 0.6, 0.35, SEAT_M - 0.05, SEAT_M)))
    return got


@makes("drinking_water", "fountain")
def fountain(o, along, out):
    return [("limestone", *_post(o, 0.95, 0.22, 10))]


@makes("telephone")
def telephone(o, along, out):
    return [("paint", *_box(o, along, out, 0.90, 0.90, 0.0, 2.40))]


@makes("bicycle_parking")
def bicycle_parking(o, along, out):
    """A row of Anlehnbuegel: DIN 79008 gives 0.75 m over the ground and 0.70 m across."""
    got = []
    for k in range(4):
        at = (o[0] + along[0] * (k - 1.5) * 0.80, o[1] + along[1] * (k - 1.5) * 0.80, o[2])
        for side in (-1.0, 1.0):
            leg = (at[0] + out[0] * side * 0.35, at[1] + out[1] * side * 0.35, at[2])
            got.append(("steel", *_post(leg, 0.75, 0.025, 6)))
        got.append(("steel", *_box((at[0], at[1], at[2]), out, along, 0.70, 0.05, 0.72, 0.77)))
    return got


def from_osm(doc, frame, z_at, facing=None, reach_m=3000.0):
    """Everything the registry knows, where OSM says one stands.

    `facing` answers "which way is the carriageway" at a point and gives the piece its
    orientation -- the one thing the surveyor did not write down."""
    out = []
    for e in doc["elements"]:
        if e.get("type") != "node":
            continue
        tags = e.get("tags") or {}
        want = tags.get("amenity") or tags.get("highway") or tags.get("man_made") or ""
        make = MADE.get(want)
        if make is None:
            continue
        x, y = frame.xy(e["lat"], e["lon"])
        if abs(x) > reach_m or abs(y) > reach_m:
            continue
        o = (x, y, z_at(x, y))
        along, outward = facing(x, y) if facing else ((1.0, 0.0), (0.0, 1.0))
        out.extend(make(o, along, outward))
    return tuple(out)
