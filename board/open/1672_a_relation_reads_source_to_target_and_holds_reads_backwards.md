Type: issue
Area: scene
Tags: naming, api

**A relation reads source-to-target, and Holds reads backwards**

The catalogue's convention is that a pair spells `source <Relation> target`: body `IsA`
kind, part `ChildOf` parent, body `DrivenBy` mind, mind `Uses` tool, mind `Assigned`
route. `DrivenBy` is deliberately passive so the sentence stays true in that direction.

`Holds` (include/outshine/Register.h:57,83 — added closing 1669) inverts it. The wiring
is `Link(held, Relation::Holds, holder)` (src/clients/Assembly.cpp:182,195), so
`TargetOf(car, Relation::Holds)` returns the GARAGE — the code reads "the car holds the
garage" and means the opposite. The very first proof written against it already trips:
test/unit/scene/AHolderKeepsWhatItHoldsAndAPrefabDoesNot.cpp:28-29 spells
`scene.Link(car, Relation::Holds, garage)` under the sentence "a standing garage HOLDS a
parked car". When the test author misreads the API in the commit that introduces it, every
future caller will.

Demand: rename to the passive form the convention uses — `HeldBy` (or `In`, matching the
grammar's `in=`/`holds=` spellings both landing on one relation). Rule row unchanged
(exclusive, acyclic, target Body = the holder; adjudicated under 1669 and correct:
inventory-in-inventory works because a bag IS a Body, and a region-instance stands as
Body today — widen TargetRoles only when regions become store citizens with a role of
their own). Rename `Named()` in src/scene/Store.cpp:24, the Assembly comment, and the two
tests. No behaviour change; the unit proofs pin the direction by name afterwards.
