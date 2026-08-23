Type: bug
Area: scene
Tags: bounded-terms

**The store's teardown and stand-up walk with an explicit stack**

1712 just paid for this lesson in Fit; the component core carries the same defect twice:

- `Store::Remove` (src/scene/Store.cpp:77) recurses into `Remove(child)` for every
  `OwnedByTarget` in-edge. Depth = the ChildOf chain's length, bounded only by the store's
  capacity. A scenario that assembles a 100k-link chain (a train, a rope, a convoy of
  ChildOf parts) tears down by blowing the stack — on the 512 KiB secondary-thread stacks
  the tree itself proves against (test/unit/actor/path/ACorridorIsFittedThroughVertices…),
  a few thousand frames suffice.
- `Store::Instantiate` (src/scene/Store.cpp:349) recurses per prefab child the same way, so
  standing UP a deep prefab dies identically, mid-copy, relying on the unwind's Removes —
  which recurse again.

Demanded: both walks carry an explicit work stack (the capacity bound is already the pool's
own), and the unit mirror gets the 1712-style proof — a chain the old recursion could not
survive, removed and instantiated on a small-stack thread, with the recursive form shown to
SIGNAL.

---

Closed -- Remove fells owned chains with an explicit stack (a visited entity defers until
its owned in-edges are gone, then Fell erases the one node), and Instantiate raises a prefab
tree from a work list, felling the ONE root on any refusal. Proven in
ADeepChainStandsAndFellsOnABoundedStack: a 16384-link owned ChildOf train assembles, fells,
and an 8192-part prefab chain instantiates on a 512 KiB thread stack. Negative control: the
recursive Store reverted SIGNALs on exactly this test.
