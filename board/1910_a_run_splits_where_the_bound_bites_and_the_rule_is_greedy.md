Type: issue
State: open
Parent: 1499
Area: actor/path
Depends: 1919
Tags: geometry, alignment

# A run that must split, splits by a rule that is argued

`Align` groups same-sign turns into one run and fits one arc to it (board:1795). When the arc
cannot hold every vertex within the declared accuracy, the run is shortened:

    src/actor/path/Alignment.cpp   for (;;) { held = BendOver(..., at, last, ...);
                                              if (held->AwayM <= withinM || last == at) break;
                                              --last; }

**Greedy from the end, one vertex at a time.** The vertex that forced the split leaves the run,
and everything after it starts a new run at the next iteration. That is A rule, and it is not
argued anywhere:

- it splits at the LAST vertex rather than at the one that deviates most, so a single outlier in
  the middle of a long run truncates everything after it instead of splitting around it
- the two halves are not balanced: the first run keeps as much as it can and the second gets the
  remainder, so a run of 40 vertices that fails by 1 mm at the end becomes 39 + 1
- nothing says whether the second run should re-fit from the split vertex or from the one before
  it, and the answer decides whether the two arcs share a tangent

This is why `Alignment` is amber on CLAUDE.md's TARGET map.

## What will be true

- [ ] The split rule is stated and argued: where a run breaks, why there, and what the two halves
      share at the seam.
- [ ] Proving case: a polyline of two true arcs of different radius, joined tangentially, splits
      at the join and reads both radii. Negative control: the greedy rule, which splits late and
      reads the first radius for both.
