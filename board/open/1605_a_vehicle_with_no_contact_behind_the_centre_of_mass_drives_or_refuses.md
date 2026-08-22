Type: bug
Area: sim

**A vehicle with no contact behind the centre of mass drives or refuses**

src/sim/Rigging.cpp, `Stand`:

- Line 51: `if (!(driven > 0.0)) { driven = braked; }` is a dead fallback. Line 71 assigns
  `mount.DrivenShare = one.AtM[2] > out.CentreM[2] ? 1.0 / driven : 0.0;` — the SAME predicate
  that made `driven` zero. When no contact lies behind the CoM, every DrivenShare is 0.0: the
  rig stands (`Stood = true`) and can never move under its own drive, silently. The refusal
  philosophy is refuse-at-assembly; a vehicle that cannot actuate its own drive function must
  refuse at Stand with a text, or the fallback must actually distribute drive over all
  contacts.
- Lines 47/52: `steered` is summed and floor-defaulted but divides nothing —
  `SteeredShare` is the raw predicate (line 70). Dead arithmetic; delete it or use it.
- Contacts exactly AT the CoM plane (`AtM[2] == CentreM[2]`) are neither steered nor driven by
  strict comparisons on both sides — a three-wheeler with a centred axle falls through both
  predicates. Name the tie-break.
