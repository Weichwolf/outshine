Type: feature
Area: corpus
Depends: 0078
Tags: oracle, perf, instrument

**A heavy scene, and the two instruments it must not be one of**

*Owner's ruling: once the small cases pass, heavy scenes — to be sure Outshine delivers, and as a case
for optimisation.* Both are wanted. **They are two questions and only one of them is expensive**, and a
case that answered both would be testing two things, which `CLAUDE.md` splits by instrument.

| question | suite | needs an oracle? | cost |
|---|---|---|---|
| **does it look right at scale** | `render` — picture bound against Cycles | **yes** | minutes to hours of Cycles per frame |
| **does it hold the frame floor** | `frame` / `scenario` — p50/p95/p99 over a moving camera | **no** | our renderer only |

**The cheap half is the one that is overdue.** `board:0058` records that the fourth constraint — *this
device at 720p60* — has **no instrument, no subject and no case**, and `test/scenario/` has no directory.
A heavy scene is exactly the subject it has been missing, and **as a cost subject it needs no Cycles at
all**: a declared camera path, a per-frame clock with its floor published, and a scene. That is the
scenario suite's first member, and it can exist before any oracle is paid for.

**The expensive half is bounded by choosing a frame rather than a path.** A correctness claim over a
heavy scene is one still against Cycles, not a sweep — and the number that decides which scene is
affordable is *Cycles seconds for one 720p frame at a converging sample count*, measured on one candidate
before a tier is planned around it. Our current cases run at **8.17 s**; Sponza is minutes and Bistro is
minutes to hours, so this is a different order of budget rather than more of the same.

**Candidates**, in ascending order of what they cost and what they prove: the rest of the Khronos
showcase — one hard material each · **Sponza**, interior with many materials and alpha-masked foliage ·
**Bistro** and **Emerald Square**, exterior with vegetation, the closest available thing to the built
world this engine is aimed at · **San Miguel**, heavy alpha-tested vegetation. Licences must be checked
per asset, and `board:0022` is already open about the fetch allow-list.

**And the field is wider than glTF**: `test/corpus/prepare.py` already converts through Blender, so any
`.blend` is a candidate source — Blender's demo files and the CC-BY Open Movies among them.

**Not before the showcase tier is attributed.** Five of the six heaviest cases we already run are outside
the picture bound with no mechanism named — `a-beautiful-game` 140.77, `corset` 189.00, `boom-box`
166.69, `lantern` 96.43, `scifi-helmet` 15.46. **More red cases with no more understanding is not
progress**, which is why this depends on `0078` and not on a date.
