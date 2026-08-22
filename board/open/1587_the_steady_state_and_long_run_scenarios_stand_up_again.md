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
