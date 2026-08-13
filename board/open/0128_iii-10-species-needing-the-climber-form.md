Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**III.10 Species needing the climber form**

*A climber needs a **host**, and the generator contract has no way to say "this grows on that". That is
a contract change, not a species.*

**Acceptance, shared by every child**: done = a render case exists in `test/render/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.
