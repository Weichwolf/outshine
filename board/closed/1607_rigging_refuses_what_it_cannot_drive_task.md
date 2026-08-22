Type: task
Parent: 1605
Area: sim

**A vehicle with no contact behind the centre of mass drives or refuses**

Kill the silent driven = braked fallback and the dead steered computation; decide the
CoM-plane contact by a declared rule, not by falling through two strict predicates; prove with
unit cases in the mirror.

---

**Closed.** The silent driven = braked fallback and the dead steered counter are gone; no
contact behind the centre of mass -- including the degenerate all-on-the-plane declaration --
is a named refusal. Proving test: unit/sim/ARigRefusesADeclarationItCannotDrive, in the gate's
119/119.
