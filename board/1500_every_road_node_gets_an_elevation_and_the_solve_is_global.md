Type: task
State: open
Parent: 1813
Area: ground
Tags: scope, osm


**Benchmark** — Neither engine solves this: both take terrain and roads as AUTHORED, already agreeing. Here the road comes from OSM without a third dimension, so the solve is ours. **The choice is mine**, and the constraint is CLAUDE.md's four plausibilities — a global solve is the only one that can hold continuity across a junction.
# Every road node gets an elevation, and the solve is global

OSM gives no z to anything: a bridge says `bridge=yes` and `layer=1`, a tunnel says
`tunnel=yes`, a ramp says `highway=motorway_link` — none says a height, and the DEM knows about
neither.

**A bridge cannot be solved locally, and that is the whole shape of this item.** Raise a deck
and its approaches must ramp; the approaches are OTHER WAYS whose far ends are pinned to the
terrain, and if the ramp is too short the constraint propagates into the way beyond that. One
solve over the whole road graph, never a fix per bridge.

**The gradient limit reveals every structure nobody tagged.** A road of a given class cannot
exceed a gradient; the DEM says what the ground does; where the two disagree, a structure is
inferred — which is what makes the tags optional rather than authoritative, and what makes a
railway (board:1499's tighter limits) the strongest inference of all.

## What will be true

- [ ] Every node of the routed graph carries an elevation from ONE global solve, with the
      residual per node published.
- [ ] An inferred structure names WHY it was inferred (gradient, crossing, tag) and its
      confidence travels with it.
- [ ] The solve is deterministic and re-derivable from (data pin, class table), never stored.
- [ ] A graph the limits cannot satisfy is a named refusal quoting the way and the gradient it
      would have needed.
