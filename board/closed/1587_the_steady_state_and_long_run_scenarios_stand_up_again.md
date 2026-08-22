Type: bug
Area: clients

**The steady-state and long-run scenarios stand up again**

`render/outshine/scenario/AnEngineInSteadyStateReturnsToTheSameLiveByteCount` and
`ALongRunHoldsItsMemoryAndItsPace` are red at HEAD, and the red predates the board:1574 work
(proven by stash A/B on 2026-08-22: identical verdicts with and without it). The refusal is
loud and geometric:

```
vertex 245 sits 3.292147 m along the view axis, inside the near plane of 3.294090 m this
placement declares
```

BoxAnimated's declared placement puts a vertex 2 mm inside the declared near plane, so Advance
refuses every frame. Suspect: a framing change since the declaration was written (board:1543
moved framing on 2026-08-21). Done when both suites stand up and their memory/pace claims are
judged again -- the fix is in the placement's derivation or the framing, never a widened near
plane without a derivation.

Found while proving board:1574's no-disk-on-relay slice; also of record: the corpus prune had
harvested Box, BoxAnimated and ABeautifulGame prepared files, which mimics this failure with
"cannot be opened" -- restored via prepare.py fetch, and only these two stayed red.

---

**Closed.** The mechanism: `Live::PlacedBounds` cached the bounds of ONE pose, and `Look` derived
the framing -- eye, near plane -- from that cache; an animating subject then walked 2 mm past the
frame-0 near plane and `ClearsNearPlane` refused, correctly, a framing whose population was one
frame of a 500-frame run. The fix widens the population: the cached bounds are the union over
every frame of the motion (the same fold `Stand` already did), computed once at first aim and
restored to the current pose -- so the derived near plane holds for every pose by the bounding
argument, not by margin. The prune-harvested corpora (Box, BoxAnimated, ABeautifulGame) were
restored with prepare.py fetch. Proving tests: the whole of `render/outshine/scenario` -- 6 of 6
PASS, including the frame-budget case, with a full windowed drive running beside it.
