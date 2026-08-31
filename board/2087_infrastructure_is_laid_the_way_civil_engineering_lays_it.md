Type: feature
State: open
Area: generators, world
Tags: infrastructure, terrain, benchmark

# Infrastructure is laid the way CIVIL ENGINEERING lays it

**Benchmark** — the two references are the wrong place to look: neither Unreal nor RAGE DERIVES a
road, both author one. The bodies that agree are the road CAD packages — **Autodesk Civil 3D**,
**Bentley OpenRoads**, **Vectorworks Landmark** — and they agree completely, which closes it.

## THE HOW, in their vocabulary

    alignment    horizontal reference line                 -> ReferenceLine          have it
    profile      vertical, along s                         -> ReferenceLine::Rise    have it
    assembly     typical cross-section, built of
                 subassemblies: lane, shoulder, kerb       -> Section                too thin
    DAYLIGHT     a link running OUT from the built edge at
                 a declared batter UNTIL IT MEETS GROUND   -> missing                the gap
    corridor     the built surface REPLACES the terrain
    surface      inside its own boundary                   -> board:2084's stamp

**Two halves of one mechanism.** The cross-section is held to what a road MAY be; everything the
terrain does beyond that edge is taken up by the BATTER, never by tilting the carriageway. That is
why a road can be level across on a hillside — the hill is absorbed by the embankment, and the
ground is then pressed to meet the whole thing.

## The numbers, each a DESIGN figure to verify against the standard beside it

| number | figure | stated in |
|---|---|---|
| minimum crossfall, drainage | 2.5 % | RAS-Q — and the A9 measures **2.76 %** on the straight (board:2078) |
| maximum superelevation | 6 % motorway | RAA — the A9's p95 magnitude is 5.8 %, which agrees |
| superelevation on a curve | `e = v²/(127R) − f` | the design equation; board:2078 MEASURED it rising 2.76 → 5.61 % with curvature |
| embankment batter, fill | 1 : 1.5 | RAS-Q |
| cutting batter | 1 : 1.5 soil | RAS-Q |

Two of them are already confirmed by a road built to them.

## What will be true

- [ ] A carriageway is level across but for its declared crossfall, whatever the ground does
- [ ] Crossfall is bounded both ways, and on a curve comes from the equation, not a constant
- [ ] A daylight link runs from each edge at the declared batter until it meets existing ground
- [ ] The ground is pressed to the corridor surface inside its boundary (board:2084)
- [ ] Negative control: a road across a measured slope tilts by no more than its crossfall. Today it
      inherits the hill — that is the defect
- [ ] The measure that shows this wrong: the gap under a carriageway, and the angle across it

## Not covered

Structures. A bridge spans on piers and a tunnel bores; neither daylights, and a daylight link off a
viaduct would build an embankment in mid-air. Both are board:2082's.
