Type: issue
Area: test
Tags: gate, sanitiser

**The sanitised arm covers the layers that parse hostile bytes — unit/ui first**

Promised at 1685's closure ("a sanitised arm for unit/ui is the future gate") and still
absent: test/run.sh:168-173 `LayerSanitiser` lists only the four render/driver layers. The
layers that PARSE hostile input run without ASan/UBSan:

- `unit/ui` — Layout.cpp is 1071 lines of line/cursor index maths over author-controlled
  markup; the 1685 defect was an ASan-hard OOB exactly here, found by a reviewer's repro,
  not by the gate.
- `unit/core` — Json.cpp's scanner and Script.cpp's ToNumber walk hostile byte ranges.
- `unit/gltf` — the hostile-bin arms of 1736 exercise attacker-shaped files; an OOB they
  provoke today is only caught if it corrupts a checked value.

Demanded: `unit/ui`, `unit/core` and `unit/gltf` join LayerSanitiser (and LayerValidation
stays render-only); the 1735 gate-bound audit re-measures with the added arms so the
headroom claim stays honest.

---

Closed -- unit/ui, unit/core and unit/gltf run their sanitised arms (LayerValidation stays
render-only, as demanded): 203 arms green, ASan/UBSan clean on the hostile-parser layers
where 1685's OOB hid. ONE case is exempt by name with its reason in the runner:
EveryByteTheHeapTakesLandsUnderATagOrUnderOther measures the tree's OWN operator new, which
ASan replaces -- sanitising it would measure ASan, not the instrument. The 1735 bound was
re-derived honestly against the new population: 98.0 / 98.5 / 100.2 s of run measured over
three warm passes, bound = worst x 1.5 = 150000 ms, derivation printed in run.sh; headroom
51 s.
