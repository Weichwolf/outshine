Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**III.17 The crop row form and the crops that ride it**

*Ranked by German acreage (Destatis 2024): wheat 2.62 Mha, barley 1.66 Mha, oilseeds 1.15 Mha, rye
0.54 Mha, grain maize 0.50 Mha, sugar beet 0.44 Mha, potato 0.28 Mha, oats 0.16 Mha.*

**Acceptance, shared by every child**: done = a render case exists in `test/render/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.
