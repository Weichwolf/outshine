Type: feature
Area: ground, generators
Tags: scope, osm, plausibility

# A reconstructed third dimension is plausible four ways

Owner direction, 2026-08-24:

> *"brücken, auffahrten, über- und unterführungen, tunnel und weitere dreidimensionale verläufe
> lassen sich zum teil nur rekonstruieren, da die osm daten dafür nicht alle information haben.
> wichtig ist, dass alle 'erratenen' strukturen geometrisch, physikalisch, statisch und
> architektonisch plausibel sind."*

and, the same day:

> *"aus osm gebaute infrastruktur muss plausibel sein und geometrisch korrekt aber nicht
> unbedingt realitätsgetreu."*

## What the tree does today

**Nothing.** `src/ground/StreetField.cpp:32` counts a tunnel and drops it:

```cpp
if (field.Num(f, "tunnel", 0.0) > 0.5) { Tunnels_++; continue; }
```

`WaterField.cpp:74` does the same. No source in the tree reads `bridge`, `layer` or `level` at
all -- `grep -rn 'bridge' src/` returns nothing. So every crossing in the world is currently a
road drawn flat through whatever it should have gone over or under, and the corridor's height
comes from the terrain beneath it.

That is visible in `board:1804`'s stills as one of the world's absences, and it is why the
Munich--Hamburg corridor cuts and fills a motorway into every valley it should bridge.

## The four plausibilities, and what each one is measured by

| | what it means | how it is measured |
|---|---|---|
| **geometric** | the course closes, is continuous in position and curvature, and does not intersect itself or what it crosses | the same seam and leap walks `ReferenceLine` already carries (`board:1774`), plus a clearance test against whatever passes beneath |
| **physical** | a vehicle drives it at the speed its class implies | the speed plan over the reconstructed course, against the class's own design speed |
| **static** | it stands: spans, piers and clearances that could carry their own load | span against a declared structural depth ratio, pier spacing, deck thickness -- all from a fetched standard, none invented |
| **architectural** | it looks like the thing it is | a still from the seat, judged as `board:1804` judges the rest of the world |

A guess that holds all four is right. One that holds three is a finding.

## What OSM does and does not carry

| carried | not carried |
|---|---|
| `bridge=yes`, `tunnel=yes`, `layer=N` (relative stacking) | deck height above the obstacle, span lengths, pier positions |
| the way's own polyline and class | ramp gradients, merge geometry, the vertical curve at each end |
| `level` for buildings | clearance under a bridge, the profile of an underpass |

So the reconstruction is a GENERATOR in the tree's own sense: `(kind, params, seed, rung)` in,
geometry out, pure. It belongs beside `Infrastructure`, not inside the corridor.

## What will be true

- [ ] A way tagged `bridge` or `tunnel` is no longer dropped: it carries its tag to the
      corridor, and the corridor's height over that stretch is the STRUCTURE's, not the
      terrain's.
- [ ] `layer` decides what passes over what where two ways cross, and a crossing whose layers
      do not order is a named refusal rather than two roads at the same height.
- [ ] The deck's clearance over what it crosses is declared per class from a fetched standard,
      the way `minRadiusM` and `maxGradient` are (`board:1784`, `board:1794`).
- [ ] Each of the four plausibilities has a proof that can go red on its own.
- [ ] Negative control per proof: the reconstruction disabled -> the road is flat through the
      obstacle and the matching claim names it.
