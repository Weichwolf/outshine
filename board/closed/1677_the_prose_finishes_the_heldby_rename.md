Type: bug
Area: scene
Regresses: 1672
Tags: naming, tests

**The prose finishes the HeldBy rename**

1672's closing claims "catalogue, store naming, assembly and both proofs renamed".
Two spots still speak the dead name:

- src/clients/Assembly.cpp:171 — the possession comment reads "Holds is possession";
  the relation beneath it is `Relation::HeldBy` (Assembly.cpp:183,196).
- test/unit/clients/AKindIsADefaultAndAnInstanceOverridesIt.cpp:85 — the CHECK
  sentence says "the crate holds the cup by Holds" while the assertion one line up
  reads `TargetOf(cup, Relation::HeldBy)`.

Two words, but they are exactly the misread 1672 was filed to kill: prose that names
the inverted relation teaches the next caller the wrong arrow. Rename both; grep
proves zero `Relation`-adjacent `Holds` outside `Instance::Holds`/`<holds what>`
(the holder-side XML spelling, correct in its own direction, stays).

---

Closed: the two prose remnants speak HeldBy (Assembly's comment, the kind test's CHECK
text); grep for the old name over src/ and test/ returns only history in board/.
