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
