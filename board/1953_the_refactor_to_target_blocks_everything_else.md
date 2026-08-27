Type: feature
State: active
Area: architecture

# The refactor to TARGET blocks everything else

**Benchmark** — Unreal and RAGE are the two bodies of evidence; this item IS the rule that TARGET holds the better of them before anything is built on it. **Taking both**, question by question, in the settled table.

**Nothing else is worked while this stands.** Not a feature, not a gap, not a finding either
reviewer files -- those are recorded and they wait. An item worked on the current architecture is
work done twice, and the second time is the one that counts.

The order, and it does not vary:

1. **Does TARGET match the best of RAGE and Unreal?** If not, TARGET is what gets repaired, and
   nothing is built until it is. A refactor toward a target that is short of the benchmark spends
   the effort and arrives somewhere that still has to be left
2. **Rebuild onto TARGET**
3. **Then, and only then, close the feature gaps**

Step 1 has been done once and moved four statements (`CLAUDE.md`): one WORLD rather than one world
space, one pre-view translation per frame, a GPU-driven frame path, and a world that streams by
cell. Each of those four is a child here, and each was measured against what the two benchmarks
actually do rather than against what sounded right.

**The evidence that this order is the right one is the hour that produced it.** A shadow that
stood in the wrong place cost a day and two wrong repairs, and the cause was that two subsystems
each subtracted an origin for themselves. Under `PreViewTranslation` the question cannot be
asked -- there is one origin, the frame picks it, and a subsystem has nowhere to put a second one.
The bug was not hard. The architecture made it hard, and every repair on that architecture buys
the next one at the same price.

- [ ] the four children stand closed
- [x] the own cases were asked ONCE, at the end: `test/outshine` 57 of 57, the drive lays 2.916 km
      and drives it, the three producers are 0 subpixels apart of 16384, and `apps/driver`'s stills
      are BYTE-IDENTICAL to the ones it wrote at 132f07d9^, before this work began
- [ ] `apps/driver` renders through the rebuilt path and its picture is judged on the stills it takes itself
- [ ] no item outside this parent was closed while it stood
