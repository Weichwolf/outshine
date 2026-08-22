Type: bug
Area: sim

**The move-2 extraction leaves no dead state behind in Journey and Rigging**

"Delete on the day you replace" -- the CorridorLay/DriveTick cuts left residue at HEAD:

- src/sim/Journey.cpp:112-119: `State` still carries `HeldAsideM`, `HaveAside`, `NearM`,
  `LostM`, `SimulatedS`, `Tally` -- dead duplicates of the fields that live in
  `DriveState` (DriveTick.h:63-74); nothing in Journey.cpp reads them, and duplicated state
  is where the next drift starts.
- src/sim/Journey.h:38-107: `struct Laid` (~70 fields) has NO consumer anywhere in the tree --
  `Lay` returns bool. A dead 70-field public struct in a header that must "read like a good
  book".
- src/sim/Journey.cpp:398-401: a `Pilot::Reins` is built, three fields assigned, then never
  used -- the live copy is built inside DriveTick.
- src/sim/Journey.cpp:366-367: `auto &corridorLaid = S_->Way; (void)corridorLaid;` and an
  empty `namespace { }` block above `Lay` (lines 149-151).
- src/sim/Rigging.cpp:83-90: the wheelbase partition files an on-plane contact
  (`AtM[2] == CentreM[2]`) under the REAR axle (`else` branch) while the share logic three
  lines up declares the rule "a contact exactly on the centre plane belongs to no axle"
  (the refusal text, line 52-55). One predicate, two partitions -- the named rule must hold
  everywhere it is spelled.

---

**Sharpened (review 2026-08-22, evening): the sweep of task 1618 left two items standing.**

- the empty `namespace { } // namespace` block this issue named still sits above `Lay` at
  src/sim/Journey.cpp:141-145 at HEAD (e5a1122).
- move 2d added new residue of the same species: Journey.cpp:145-152 copies
  `driveTo->FromLatDeg` into `fromLatDeg0` and then `fromLatDeg0` into `fromLatDeg` (four
  doubles, twice each, plus `zoom`→`kZoom`) — a two-step rename with no reader in between.

Stays open until both are gone.

---

Closed (review 2026-08-22, night round): every named residue is provably gone at HEAD.
Journey.{h,cpp} left the tree with move 2(e) (d5b69e60, 90b85f44) and took the dead State
fields, the 70-field `Laid` struct, the unused `Pilot::Reins`, the empty namespace block and
the two-step rename with it -- DriveAssembly.cpp:63-67 copies `driveTo->FromLatDeg` into its
locals in ONE step. Rigging.cpp now partitions with strict inequality everywhere the rule is
spelled: DrivenShare/SteeredShare (lines 84-85) and the wheelbase axle split (lines 91-97,
`else if (one.AtM[2] > out.CentreM[2])`) both leave an on-plane contact with no axle, exactly
as the refusal text (lines 61-64) declares. Task 1618 closed earlier. Proof: fast gate
122/122 at 90b85f44.
