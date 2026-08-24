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

## What the SOURCE carries, measured (2026-08-24)

Not what OSM carries -- what the provider this tree actually reads carries. Counted over the
28 752 ways harvested for the Munich--Hamburg route:

```
NOTE ways the data marks as a bridge          = 0 ways
NOTE ways it marks as a tunnel                = 0 ways
NOTE ways that declare a stacking layer       = 0 ways
NOTE distinct tag keys the vector tiles carry = 2 keys
KEY kind
KEY rail
```

**Two keys.** The Versatiles vector tiles carry `kind` and `rail` and nothing else -- no
`bridge`, no `tunnel`, no `layer`, no name, no lane count, no speed. So the first box as filed
is not achievable from this provider at all: there is no tag to carry.

## But the topology carries it, by omission

OSM's own convention is the signal: **two ways that cross AT GRADE share a node; two ways that
cross grade-separated do not.** That survives the tiling, because it is geometry rather than a
tag.

The tree computes half of it. `Network::Weave` snaps points within `SnapM` into one node, so
ways that share a point become a junction (`JunctionCount()` counts nodes with more than two
edges). It never computes a SEGMENT crossing, so two ways whose polylines intersect in plan
without sharing a point simply pass through each other with nothing recorded -- which is
exactly the grade separation, and exactly what is missing.

That makes the reconstruction's input geometric and provider-independent, which is better than
a tag would have been.

## What will be true

- [ ] The network computes plan-view segment crossings that are NOT junctions. Those are the
      grade separations the source encodes by omission, and their count is published.
- [ ] Which of the two ways passes over is decided and the decision is written down -- from the
      classes, the gradients, or a stated rule -- because OSM's `layer` is not available to
      answer it.
- [ ] The corridor's height over a reconstructed span is the STRUCTURE's, not the terrain's,
      and the clearance beneath is declared per class from a fetched standard the way
      `minRadiusM` and `maxGradient` are (`board:1784`, `board:1794`).
- [ ] Each of the four plausibilities has a proof that can go red on its own.
- [ ] Negative control per proof: the reconstruction disabled -> the road is flat through the
      obstacle and the matching claim names it.

## Withdrawn from this item

The boxes about `bridge`, `tunnel` and `layer` tags. They cannot be written against a provider
that carries two keys, and filing work against data the tree does not have is the defect
`board:1786` describes in another form. If a provider that carries them is ever declared, this
paragraph is where the argument for it starts.

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
