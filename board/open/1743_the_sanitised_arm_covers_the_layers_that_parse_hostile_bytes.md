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
