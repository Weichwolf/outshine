"""EVERY BODY A REAL EXTRACT CARRIES, AGAINST THE GEOMETRIC INVARIANTS.

The twin checks what it DRAWS -- it culls to the frame, and a body nobody sees cannot spoil a
picture. The claim that every body a place holds is closed, wound and solid is a different claim
and it needs its own run, because answering it means building all of them: 1 054 bodies of
Rothenburg are 69 s where the frame's fourteen are 0.3 s.

It is also the only instrument that can see what the 41 synthetic cases cannot. All 41 are green
in every configuration measured, because a synthetic footprint is a rectangle somebody chose; the
defects live in the plans a surveyor drew.

MEASURED 2026-09-07 on OldTown, and what each round bought:

    no crease constraint on a roof     381 of 5 709 bodies open
    `skeleton` (every offset)          291   -- closed none, opened 80
    `axis` (the ridge line)            198   -- the gabled failures go 167 to 0

and what is left is board:2159: 172 flat-roofed bodies with 0 open edges and exactly 1 edge
carrying three faces.
"""
import collections
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "roads"))
sys.path.insert(0, str(HERE / "buildings"))


def run(name="OldTown"):
    import places
    place = next(p for p in places.PLACES if p["name"] == name)
    frame = places.Frame(place)
    doc = places.overpass(place, places.BUILT_REACH_M)
    bodies, dropped = places.buildings_of(place, frame, doc, [])
    open_by, wound_by, thin = collections.Counter(), collections.Counter(), 0
    for b in bodies:
        if not b.watertight():
            open_by[b.roof] += 1
        wrong, degenerate, _ = b.winding()
        if wrong or degenerate:
            wound_by[b.roof] += 1
        if b.volume() <= 0.0:
            thin += 1
    bad = sum(open_by.values())
    print(f"{name:12s} {len(bodies):5d} bodies read, {dropped} dropped")
    print(f"  not watertight {bad:5d} ({100.0 * bad / max(len(bodies), 1):.1f}%)  "
          f"{dict(open_by.most_common(6))}")
    print(f"  wound wrong    {sum(wound_by.values()):5d}  {dict(wound_by.most_common(4))}")
    print(f"  no volume      {thin:5d}")
    return bad + sum(wound_by.values()) + thin


def main(argv):
    worst = 0
    for name in (argv or ["OldTown"]):
        worst = max(worst, run(name))
    print(f"\nsweep {'RED ' + str(worst) + ' bodies fail an invariant' if worst else 'ok'}")
    return 1 if worst else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
