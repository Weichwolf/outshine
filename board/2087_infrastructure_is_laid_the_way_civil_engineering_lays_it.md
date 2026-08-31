Type: feature
State: active
Area: generators, world
Tags: infrastructure, terrain, benchmark

# Infrastructure is laid the way CIVIL ENGINEERING lays it

**Benchmark** — neither Unreal nor RAGE DERIVES a road; both author one. The bodies that agree are
the road CAD packages: **Civil 3D**, **OpenRoads**, **Vectorworks Landmark**. They agree completely.

## How

    alignment   horizontal reference line          ReferenceLine        have
    profile     vertical, along s                  ReferenceLine::Rise  have
    assembly    cross-section of subassemblies:
                lane, shoulder, kerb               Section              too thin
    DAYLIGHT    runs out from the built edge at a
                declared batter until it meets
                existing ground                    --                   MISSING
    corridor    the built surface replaces the
    surface     terrain inside its boundary        board:2084's stamp

## Why

The cross-section is held to what a road MAY be; the hillside is absorbed by the BATTER, never by
tilting the carriageway. That is how a road stays level across on a slope — and why it then has to
press the ground to meet it.

## Numbers, each to verify against the standard beside it

| number | figure | stated in |
|---|---|---|
| minimum crossfall | 2.5 % | RAS-Q; A9 measures **2.76 %** on the straight (board:2078) |
| maximum superelevation | 6 % motorway | RAA; A9's p95 is 5.8 % |
| on a curve | `e = v²/(127R) − f` | board:2078 measured 2.76 → 5.61 % with curvature |
| batter, fill / cut | 1 : 1.5 | RAS-Q |

## What will be true

- [ ] Level across but for the declared crossfall, whatever the ground does
- [ ] Crossfall bounded both ways; on a curve from the equation, not a constant
- [ ] A daylight link from each edge at the batter, until it meets ground
- [ ] The ground pressed to the corridor surface (board:2084)
- [ ] Negative control: a road across a measured slope tilts by no more than its crossfall
- [ ] Not covered: bridges and tunnels do not daylight — board:2082
