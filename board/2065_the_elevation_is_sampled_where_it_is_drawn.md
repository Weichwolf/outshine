Type: bug
State: open
Area: world, render
Tags: measured, picture

# The elevation is sampled WHERE IT IS DRAWN

**Benchmark** — Unreal: `ULandscapeComponent` streams heightmap mips and the resident mip is
chosen against the component's screen size, so the finest heightmap a component holds is the one
it renders. RAGE: terrain heightfield LOD is streamed at the resolution the block draws.
**Both agree and the matter is closed**: you sample the ground at the resolution you draw it.
Neither invents vertices between postings and neither carries postings it cannot draw.

## Measured at Heidelberg, 49.4147 N

`GroundStack.cpp:46` opened the elevation stream at a hardcoded `surface.Z = 12` while
`TerrariumDem.cpp:27` declares `MaxZoom = 15`.

| quantity | value |
|---|---|
| the elevation's own posting | **99.457 m** |
| the drawn mesh's vertex spacing | **24.864 m** |
| vertices drawn per elevation sample | **4 x 4** |

Fifteen of every sixteen drawn vertices were interpolated from a ground nobody measured there.

## What it cost, and it was not only the shape

- **The terrain read as smooth dunes** rather than the Neckar valley, because a 100 m posting
  cannot hold a valley wall.
- **The whole picture was DESERT BROWN.** The classifier reads SLOPE (`Forest.cpp:96`,
  `AlpineLimit::BareBySlope`), and a slope taken across 100 m is flat everywhere, so the
  Koenigstuhl -- a beech forest -- came out as bare earth. One constant produced the flat
  ground AND the wrong land cover.
- **Buildings sat in it.** `buildings: seated BELOW the ground they stand on` read 1 and the
  eye read many, because that measure compares the seat against the least-squares PLANE through
  the footprint's corners and the building is drawn against the mesh.

## After, same place, same hour

| | before | after |
|---|---|---|
| posting | 99.457 m | **24.864 m**, the mesh spacing exactly |
| seated below ground | 1 | **0** |
| frame p50 | 1.72 ms | 1.75 ms |
| streamed, cold cache | 0.6 s | 20.1 s (one time) |
| streamed, warm cache | 0.6 s | **1.5 s** |
| Shibuya p50 / RSS | -- | 9.80 ms / 2.45 GB, no kill |

## What will be true

- [ ] The stream's zoom is DERIVED from the elevation source's declared `MaxZoom` and the
      patchwork's own grid, never written down: a stream tile carries `kStreamGrid` postings and
      a patchwork tile lays `kPatchGrid - 1` intervals, so the stream opens exactly one zoom
      below the finest tile. A `static_assert` holds the two grids to that relation so a change
      to either fails the build instead of quietly re-coarsening the Earth.
- [ ] Negative control: the derived zoom reproduces the hand-set 14 -- same posting, same
      picture digest -- so the derivation is proven to be the thing that chose it.
- [ ] NAMED AND NOT FIXED HERE: the picture digest differs between a COLD and a WARM cache
      (236b28e5 against a2d6cd59) and is stable across three warm runs. Determinism holds given
      resident data and not across a cold start, which is a streaming boundary this tree has not
      declared. It belongs to whatever item owns `make shots`' preconditions.
