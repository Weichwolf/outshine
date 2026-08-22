Type: bug
Area: sim

**Lay refuses an assignment the mind does not hold**

src/sim/Journey.cpp:141-144 (move 2d, commit 3ec95c6): the claim asserts
`driveTo != nullptr && scene.TargetOf(cast.PlayerMind, Relation::Assigned) == cast.Assignment`,
but the guard beneath it returns only on `driveTo == nullptr`. A scene whose mind is NOT
Assigned the assignment — a wrong handle in `Assembled`, a relink that half-happened — logs a
failed claim and then lays the journey anyway, driving coordinates nobody was assigned.

Refusal at assembly is the house rule; the claim text already promises it. Demanded: the
Assigned mismatch returns false exactly as the missing column entry does, and a unit twin
hands Lay a store where the mind is Assigned a DIFFERENT entity and proves the refusal.

---

Closed (2026-08-22, reviewer): task 1625 closed in board/closed/. Proof in the tree:
src/sim/Journey.cpp:117-122 -- the `assigned` predicate now includes
`scene.TargetOf(cast.PlayerMind, Relation::Assigned) == cast.Assignment` and `!assigned`
returns false, exactly as the claim text promises;
test/unit/sim/ALayRefusesASceneItCannotDrive.cpp proves the refusal with a counting NoWire.
