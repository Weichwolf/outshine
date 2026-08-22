Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.12 Traffic and the parked world**

---
## The count
*Recounted mechanically 2026-08-12 after the first three tests landed. The block this replaces said
1974 / 1504 / 233 and was wrong on the day it was written: the method it declared — one pass over the
`- [ ]` / `- [x]` lines — does not produce those numbers over any state this file has had. Two things
are corrected with it. The method now says what it excludes, because the block's own table rows and
the legend at the head of this file match the pattern and were being counted as features; and the
count is taken at the top of a bullet (`^- [`), so an indented sub-item cannot inflate it. **A count
nobody recomputes is a claim**, and this one had drifted twice.*
*Recounted again 2026-08-12 after § I.19's shader ruling and § I.26.1–I.26.6 landed — 96 new lines, all
unticked. **The previous block's total was right and its split was wrong**, which is the drift this
file keeps warning about: it read 227 built / 1405 not built over a correct total of 1632, where the
committed file carried **244 / 1388**. Seventeen ticked lines were counted as unticked. The total is
the number nobody checks against reality and the split is the number everybody quotes, so the drift
landed in the worse of the two. Method, stated so it can be re-run: count lines matching `^- [` at the
top of a bullet; the legend at the head carries none and this block's rows are table rows, so neither
needs excluding after all.*
| | |
|---|---|
| Lines in this file | **2937** |
| Feature lines — `^- [` at the top of a bullet | **1825** |
| `- [x]` built and checked | **244** |
| `- [ ]` not built | **1581** |
| Band 0 — residency | 78 |
| Band I — engine (§§ I.1–I.17 plus the library sections §§ I.18–I.26) | 645 |
| Band II — world | 156 |
| Band III — vegetation | 459 |
| Band IV — buildings and infrastructure | 346 |
| Band V — vehicles | 141 |
| `NO SUBSTITUTE` | 12 |
| `REFUSED` | 17 |
| `TILE` | 5 |
| `TOOL` | 18 |
| `UNSURE` | 3 |
*The 1 796 are not the suite's size, and there is no longer one suite to size. § I.26.9 splits the tests
into three by instrument — **render**, **scenario**, **unit** — and § I.26.11 designs the render matrix,
whose enumeration is mechanical and whose expected instantiation is **≈ 200 render cases for the renderer
stage alone**, deliberately not written out as 200 lines because a hand-typed list would have holes and
no way to prove it does not. A count of requirement lines and a count of tests are different numbers and
this file holds the first.*
land templates — and every one of the sixteen rides the single growth form the generator can shape. The
engine's own machinery accounts for most of the rest. Nothing in bands IV and V beyond the footprint
prism, the way widths and their point queries is ticked, and Band V is entirely unticked.
wound, unit-normal and in-range bark invariants over every declared species
(`test/outshine/unit/generators/draw/GrownBarkIsAClosedMesh.cpp`), the planar geodesy's round trip and its priced
approximation (`test/outshine/unit/core/PlanarGeodesyHoldsToItsScope.cpp`), and the harness's own red
(`test/outshine/harness/ExpectFail.cpp`). The twelve gates beside them are structural. § I.21 carries the
classification that says how many lines ever can be tested — **537 with what is in the tree, 707 more
once each band's reference table is written down, 124 behind a device, 62 behind motion, 6 behind a
sense we cannot instrument** — and that reading was taken over 1436 lines, which is the population it
must be retaken over when § I.21's own line is worked.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Parked vehicles as dressing along a residential street *(was 1015)*
- [ ] Parking bay occupancy from the street class and the time of day *(was 1016)*
- [ ] Traffic spawned on street centrelines at a density derived from the road class *(was 1017)*
- [ ] Lane following and junction rules *(was 1018)*
- [ ] Traffic light obedience *(was 1019)*
- [ ] Yield at a pedestrian crossing *(was 1020)*
- [ ] Headlights on at night, and it is the single most visible night-time element after street lighting *(was 1021)*
- [ ] Vehicles despawned outside the observer's reach without their *knowledge* becoming observer-dependent *(was 1022)*
- [ ] Agricultural machinery in a field in season *(was 1023)*
