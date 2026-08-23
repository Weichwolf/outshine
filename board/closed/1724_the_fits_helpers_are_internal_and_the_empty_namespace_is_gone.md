Type: bug
Area: actor
Tags: hygiene

**The fit's helpers are internal and the empty namespace is gone**

The 1713 refactor left residue in src/actor/path/Fit.cpp:

- Lines 8-11: an EMPTY anonymous namespace, the husk of the helpers that moved.
- Lines 82-88: `ShiftShare` and `TangentShare` are defined at `outshine` namespace scope in
  a .cpp with no header declaration — external linkage for two internal helpers. Any future
  TU defining the same common names collides at link (or worse, ODR-merges silently), and
  the symbols leak from the library for nothing.

Demanded: the helpers move into the (one) anonymous namespace, or gain a declaration in the
header if a twin test wants them by name; the empty namespace dies.

---

Closed -- ShiftShare/TangentShare moved into the one anonymous namespace (internal linkage,
no leaked symbols, no ODR surface) and the empty husk namespace died. Proven by the build:
unit/actor/path 12/12 green.
