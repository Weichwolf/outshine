Type: bug
Area: client
Tags: boundary, persistence

**Restore refuses a row the scene never held and a value with a tail**

1702's whole-or-nothing repair stands (dry-run onto staged rows, commit writes whole rows,
Column::Put cannot fail single-threaded past a dead entity). Two boundary gaps remain in the
same function, and a save file is disk input — the defensive side of the boundary rule.

- `src/clients/Engine.cpp:323-325` — the value parse checks `scanned.ec` but never
  `scanned.ptr == line.data() + line.size()`. `car.x 1.5garbage` restores as 1.5 with the
  tail silently eaten; `inf`/`nan` spelled by hand are also accepted and poison the columns,
  though Save (`Engine.cpp:255-257`) can never write either.
- `src/clients/Engine.cpp:320-331` — the dry run validates InstanceNamed and TraitKey but not
  that the HOLDER carries the pair: `Traits::Put` (include/outshine/Traits.h:25) APPENDS an
  absent key, so a hand-edited save grafts a trait onto an instance that never declared it.
  Save refuses the symmetric case loudly (`Engine.cpp:249-254`: "a save of a missing value
  would load as a lie"); Restore must refuse with the same voice.

Proof: a save with a trailing-junk value refuses naming the line; a save naming a pair the
assembled scene does not hold refuses; both leave the columns untouched (the 1702 fixture
extends).

---

Closed -- the value must consume its whole tail and be finite (Save writes neither a
trailing rune nor an inf, so a line carrying one is an edit and refuses naming the line),
and the dry run demands the SEAT before the value moves: Restore refuses to graft a trait
the holder never declared, the same voice as Save's missing-value refusal from the other
side. The budget refusal became unreachable behind the seat check and the proof follows the
truth. Proven in ASaveIsAFunctionOfTheDeclaration (1.5garbage refuses naming the line, a
hand-spelled inf refuses, the graft refuses with "never declared", every refusal leaves the
column at 0.5). Negative control: pre-fix Engine goes 1 FAIL on exactly these arms.
