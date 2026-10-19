# Original hull controls and far-voxel cost preflight (#12)

This extends the rejected first [per-source layered proposal](source_far_assets.md).
It changes no native code or renderer admission. The next useful voxel experiment
belongs at very small projected detail, after complete sampling and shader fitting;
the preflight is not a voxel renderer or a ready asset.

Reference correction: the historical 432-view counts below use the previous
automatic PyEmbree query. [The corrected reference](source_billboard.md) fixes a
coincident front/back counterexample. Four real controls agree; the full old batch
has not been rerun, so its image/occupancy counts remain historical measurements.
Original native stream bytes and the point-pair projection formula are unaffected.

## Original inputs and controls

Nine additional source controls preserve original index/form bytes, source ordinal
and generated parts. Seven come from three existing Vienna cohorts; the other two
come from original cached Alpine and Flensburg inputs. These are three European
regions, not a worldwide sample. Anonymous native IDs remain anonymous; the control
identity is original cohort plus ordinal, without cross-cohort deduplication.
First-in-cell and no cached hip plan remain limitations of the bounded adapter.

The unchanged SHA-pinned native probe emits Fine and actual Shell. Its independent
native/GDB JSONs agree without timing fields, and the six captured streams agree
between whole/cell invocations for all 18 selected variants. Generation error 0
requests Fine. Error 4 selects Shell for eight controls; the complex 6d41/1410
control uses 512. A fixed generation parameter is **not** a detail-type guarantee:
512 selected Massed for six initial controls. The unchanged GDB validator correctly
rejected those as Shell (terminal 1); their native process itself exited 0.
The successful controls do not relabel Massed or loosen runtime admission.

| Cohort / source ordinal | Fine triangles / bytes | Shell triangles / bytes |
| --- | ---: | ---: |
| 035e2150 / 1635 | 2,481 / 146,748 | 359 / 21,012 |
| 035e2150 / 4005 | 3,744 / 276,048 | 160 / 10,928 |
| 035e2150 / 4092 | 2,902 / 172,752 | 26 / 1,584 |
| 33764a6b / 82 (Alpine) | 996 / 58,320 | 56 / 2,976 |
| 6d41e63e / 1410 | 9,696 / 875,584 | 204 / 14,320 |
| 6d41e63e / 58 | 416 / 24,744 | 76 / 4,464 |
| a169a8d0 / 285 (Flensburg) | 1,214 / 72,072 | 48 / 2,448 |
| bbd9aab7 / 60 | 1,940 / 114,000 | 88 / 4,512 |
| bbd9aab7 / 80 | 364 / 27,072 | 72 / 4,944 |

Bytes are original `Raised::UsedBytes`, not final cache size or GPU residency.
Totals across these controls are 23,753 triangles/1,767,340 B versus
1,089/67,188 B. This compares existing native products, not a new frame gain.

## Images and appearance boundary

432 same-pose views retain 1280×720 and each original focal length: six orbit
angles, 0/32 m translation, and distances of 4/8/32/128 source extents. Orbit
distance is a test input, not a runtime profile change. The final distance level
ranges from 3,101 to 26,103 m; the elevated isolated views do not test terrain
occlusion or a ground-level city's visible horizon.

Fine covers 2,524,888 pixel samples across those views. Shell loses seven and
adds 29; material-program codes differ in 298,656 samples. These sums are neither
unique world points nor conservative coverage. Translated tall-building, Alpine
and Flensburg panels were actually opened. Two lights are evaluated dynamically
with native normal/colour factors, **not native PBR or procedural facade UVs**.
The diagnostic's flatter Shell windows do not establish a native facade defect.
Near/far material continuity and complete runtime images remain unproved.

## Far-voxel preflight

[Far Voxels, Gobbetti/Marton, SIGGRAPH 2005](https://www.crs4.it/vic/data/papers/sig2005-farvox.pdf)
uses small projected voxels, visibility-aware samples and fitted directional
normal/reflectance models with dynamic light. Pages 3–5, figures 3–6 and the
fitting equation were visually inspected. Its diffuse/static-model assumptions
and sampling limitations do not establish Outshine's PBR or coverage contract.
The author PDF remains temporary; only the source link is published here.

`far_voxel_preflight` counts distinct grid cells reached by actual Fine pixel rays.
A common pitch per source/distance level includes all test orbit angles,
translations and the existing 64 m eye-reuse cube. The existing projection bound
checks point pairs within each voxel: 384 eligible cases have maximum bound
0.499408 px; 48 close cases reject the reuse domain. That proves neither a
reconstructed surface nor its material, opacity or lighting error.

The accounting assumption is 14 B per observed cell (6 position, 4 normal,
4 colour), excluding shader classes, UV/materials, source memberships, opacity,
hierarchy and fallback. Per-view observed cells are **not complete cached assets**
or a globally minimal rendering. At distance level 8, their maximum per-view
counts range from 2,229 to 11,837 and this payload estimate exceeds every Shell.
At level 128, they range from 12 to 49 (168–686 B), below each Shell, before the
missing costs. The plot records that crossover, not a performance or quality win.
Coordinate arrays and observed-cell work are counted; library sort/allocations
remain unspecified. Python control-process peaks reach about 250 MiB, not GPU use.

Next: complete occupancy/source memberships and a dynamically lit fitted far
product on the small projected cases, compared with Shell using the same poses.
Full material, disocclusion, continuous near/far, shadow/world and device checks
remain required. Unvalidated image products retain Fine fallback. No new native
pipeline, export-all gate or mesh-proof/LDI-resolution sweep follows this preflight.

## Reproduction

Use `.venv` with NumPy, Trimesh/Rtree, Pillow and Matplotlib. Original cache inputs
can be copied with `export_native_building_selection_inputs.py` in read-only mode;
the one-source and frozen native/GDB controls follow [the source contract](source_far_assets.md).
With a verified Fine/Shell pair and fresh explicit system-temp outputs:

```sh
nice -n 10 .venv/bin/python -W error test/experiments/render/compare_source_far_views.py \
  --fine "$FINE" --hull "$SHELL" --distance-extents 4 8 32 128 --output "$TEMP_VIEWS"
.venv/bin/python test/experiments/render/plot_far_voxel_preflight.py \
  --input "$TEMP_VIEW_PARENT" --output "$TEMP_PLOT"
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p 'test_source_far*.py'
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_far_voxel_preflight.py
```

The preflight does not alter the prior layered comparison: all 24 old non-timing
cases and PNG bit patterns remain identical. The 168 initial new hull cases also
retain their non-timing results after adding the farther views and counters.
