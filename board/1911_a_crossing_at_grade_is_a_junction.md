Type: issue
State: open
Parent: 1813
Area: ground, sim
Tags: measured, osm, topology

# A crossing where neither way is a bridge or a tunnel is a junction

The crossing sweep finds them, counts them and throws them away:

    src/sim/DriveAssembly.cpp:216   const auto sweep = roads.Crossings(crossings);
    src/sim/DriveAssembly.cpp:223   say.Number("places two ways cross in plan without sharing a node", ...)

**6340 places** on the shipped f31 extract, and nothing is done with any of them. Measured at
b4adb48d, after the tie index was repaired (board:1894):

| | |
|---|---|
| nodes after snapping | 45 248 |
| junctions among them | 15 143 |
| pieces the graph falls into | 563 |
| nodes in the largest | 42 233 (93 %) |
| **places two ways cross in plan without sharing a node** | **6 340** |
| ways the data marks as a bridge | 88 |
| ways it marks as a tunnel | 309 |

Until b4adb48d the bridge and tunnel counts were both ZERO -- the vector reader could not decode
a boolean -- so there was no evidence to weigh and no rule could be written. There is now.

## The rule this item has to decide, and it is not free

A routing engine trusts the topology: OSRM and Valhalla do not join two ways that cross without
a shared node, because a false junction invents a turn that no vehicle can make. **outshine is
not a routing engine.** CLAUDE.md: infrastructure built from OSM is PLAUSIBLE and geometrically
correct, never necessarily true to the real road -- the data is a source of SHAPE. A world where
two crossing streets meet is more plausible than one where a graph falls into 563 pieces.

Against that: 88 bridges and 309 tunnels say some of those 6340 are genuinely NOT junctions, and
a motorway fused to a footpath is a finding of its own. The schema carries no `layer` key at all
-- the twelve keys are kind, rail, link, oneway_reverse, bridge, oneway, tunnel, surface,
service, bicycle, horse, tracktype -- so bridge and tunnel are the whole of the evidence.

## What will be true

- [ ] A crossing is judged, not counted: neither way a bridge, neither a tunnel, and their kinds
      compatible -> a node is inserted and both ways are split on it.
- [ ] The judgement is published as numbers -- crossings joined, crossings left alone, and why --
      so a fused motorway is visible rather than silent.
- [ ] Proving case: two ways crossing at grade with no shared node become one junction and a
      route crosses between them; the same pair with one way tagged as a bridge does NOT, and the
      route still refuses. Negative control: the sweep's result discarded as it is today, and
      both routes refuse.
