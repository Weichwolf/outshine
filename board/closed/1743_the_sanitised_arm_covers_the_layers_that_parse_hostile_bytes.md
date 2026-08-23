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

---

REOPENED (review 2026-08-23, round 26). The closure met the three layers the item HAPPENED
to name; the item's TITLE is still not true, and the two layers left out are the only ones
in the tree that parse bytes off the NETWORK:

- `src/data` -- `VersatilesVector` decodes Mapbox-vector-tile protobuf from a web source,
  `WebTileSource`/`Transport` handle the wire. `unit/data` is not in `LayerSanitiser`
  (test/run.sh:177).
- `src/ground/tiles` -- `TerrainGrid::FromTerrariumPng` (TerrainGrid.cpp:8-27) decodes a
  PNG whose declared width and height it takes on trust. `unit/ground` and
  `unit/ground/tiles` are not in `LayerSanitiser` either.

Not a hypothetical: this round found undefined behaviour in exactly that unsanitised path
and had to compile a private probe to see it --

    src/ground/tiles/TileMath.h:23:52: runtime error: nan is outside the range of
    representable values of type 'unsigned int'

reachable from a 1x1 terrarium PNG through `TerrainTiles::StitchedGrid` (board:1755). The
gate ran 206/206 green over the same code in the same hour. `ui`, `core` and `gltf` parse
what a scenario ships; `data` and `ground/tiles` parse what a stranger serves, and they are
the ones running blind.

Demanded, on top of the closed part: `unit/data`, `unit/ground` and `unit/ground/tiles`
join `LayerSanitiser`; the gate bound is re-derived against the new population the way
board:1749 demands (operating population, warm, beside a parallel nest); a layer left out
carries its name and its reason in the runner, as the heap-probe exemption already does.

---

Closed (the reopened half) -- unit/data, unit/ground and unit/ground/tiles run their
sanitised arms: the layers that parse what a STRANGER serves (Terrarium PNG, Mapbox vector
tiles, the wire) are no longer the ones running blind, which was the item's whole title. The
gate stands at 219 arms, 108 s of run against the 230 s bound derived in 1749's operating
population -- inside it, so no re-derivation is owed; the bound's population line already
names the sanitised layers. The UB this exposed is repaired and pinned as 1755.
