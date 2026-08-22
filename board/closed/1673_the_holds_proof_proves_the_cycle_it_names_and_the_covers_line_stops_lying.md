Type: bug
Area: scene
Tags: tests, vacuous-proof

**The Holds proof proves the cycle it names, and the Covers line stops lying**

Two defects in the proofs that closed 1669, both in the delta:

1. **The acyclic check is vacuous.**
   test/unit/scene/AHolderKeepsWhatItHoldsAndAPrefabDoesNot.cpp:50-52:

       CHECK(!scene.Link(garage == kNoEntity ? car : garage, Relation::Holds, car) ||
                 !scene.Link(car, Relation::Holds, car), ...)

   `garage` was Removed at line 42 but its Entity VALUE is not kNoEntity, so the ternary
   picks `garage`; `Link` on a dead end refuses ("needs both of its ends standing"),
   `!false` short-circuits the `||`, and `Link(car, Holds, car)` NEVER EXECUTES. The
   check labelled "the Acyclic rule guards the relation" proves only that a dead entity
   cannot link — which ARemovalKeepsEveryIndexTrue already owns. The self-hold and the
   two-hop cycle (A holds B, B holds A — the walk at src/scene/Store.cpp:209-216 does
   refuse it, exclusivity makes the walk sound) are asserted NOWHERE for Holds.
   Fix: drop the ternary, link two live bodies into a two-hop cycle and a self-hold,
   assert both refusals name "may not close a loop".

2. **The Covers line contradicts the code above it.**
   test/unit/clients/AKindIsADefaultAndAnInstanceOverridesIt.cpp:174-177 still claims
   "holding and standing-in are one ChildOf" — the same commit (fd71b908) rewrote the
   checks to `Relation::Holds` and left the coverage claim stating the design 1669
   overturned. The Covers register is the claims ledger; a ledger entry describing the
   dead design is a lie the claims audit will repeat.

---

Closed: the vacuous check is a real one -- Link(car, HeldBy, car) alone must refuse (no
short-circuit alibi), and the two-hop cycle (the bag in the pouch it holds) refuses beside
it; the Covers lines in both proofs say HeldBy and stopped describing the relation the
commit had already replaced.
