# Demanded spatial-crown foliage: bounded source and rejected shortcuts

Issue #697 answers the concrete cold middle/far-product request in #23.
`spatial_group_source.py` connects the saved spatial crown to the existing
`group_cloud.py` and `foliage_views.py` experiments. Requested hosts can become
spatial charts without building the complete near crown. This delivers a bounded
experiment, not an admitted forest LOD or a new native/cache interface.

## Source, demand and lineage

The existing mature-oak study has 5472 skeleton nodes, 1301 tips, 4301 foliage
hosts and 90000 leaves, eight variants with 11250 leaves each. Its leaf-plan
identity remains `029f1af642d33a6aad5ba2adc640ce77ce7ce9f4505770e78800c18342c59d34`.
The original saved table SHA256 remains
`393a93ae346e395351c38d134041d3d2660d109e14f4145b828e7d3a83a34fd3`.

Loading reads positions/parents and the lazy NPZ `leaf_variants` member; it does
not read full pose members, wood or near-leaf meshes, run growth or call the
whole-crown table producer. Budget refusal precedes Shoot.detail and Blade.mesh.
An accepted host generates its original 20/21 poses. Eight shared 8×1 Blade
prototypes have 272 triangles total. Host-local assembly retains at most one
source pose group; callers own finished requested geometry/charts.

Original global leaf IDs, variants, anchors and analytic physical leaf areas
remain on the product. Analytic area is distinct from faceted mesh area.
Blade recipes and the original procedural material JSON/map hashes/qualitative
adapter hash bind the catalog identity
`62a49f3c918384dba7633ec6e13a3f48560c828df682c0745cea7e0e8115015d`.
The original material contains authored pigment/height and a qualitative thin
shader, **no calibrated botanical or aggregate R/T**. Coverage uses alpha;
legacy colour/lighting comparisons use `organ_colour` and their old cosine proxy.
Retaining original material inputs does not turn that proxy into source shading.

`spatial_source_replay.py` independently prepares six files totalling185651B:
comparison metadata, positions/parents, a variants-only NPZ and original material
JSON/two maps. No near geometry or saved poses are available in that directory.
The cold reader still produces the same groups. A separate verifier then loads
the original pose table: all90000 origins/frames/variants are exact. Four actual
host meshes agree with independently transformed original rows within
1.11023e-16m. Reordered/repeated requests and finished diagnostic NPZ reads agree.
The NPZ read is a research replay, not a proposed Outshine cache format.

## Existing methods on actual source hosts

[Billboard Clouds, SIGGRAPH2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf)
is the local starting point; PDF page4 was inspected optically. Existing greedy
normal/depth partitions and unlit angular charts are reused unchanged. Four
source hosts17/2178/3925/5471 contain83 leaves: source680–714 triangles/host,
spatial17–19 charts or34–38 triangles, selected angular card2 triangles.
All leaves belong to exactly one spatial chart, with its existing15mm vertex
projection guard. This guard is not a coverage or lighting bound.

Twelve independent directions include both sides and grazing elevations.
Every method uses the same viewport from the union of candidate/source vertices,
8× supersampling and4×4/8×8/16×16 output footprints:432 comparisons total.
The16-view alternative is every fourth entry of the64-view set, not an optimal
independent16-view distribution. Metric: sum absolute coverage difference
divided by sum per-pixel maximum coverage; lower is better.

| Footprint | Spatial mean / max | 16-subset mean / max | 64-view mean / max |
|---|---:|---:|---:|
| 4×4 | .07022 / .17176 | .25681 / .62846 | .13475 / .21494 |
| 8×8 | .06930 / .13076 | .29205 / .63844 | .17687 / .27712 |
| 16×16 | .07410 / .13298 | .38573 / .63617 | .25872 / .36712 |

Spatial charts preserve these tested footprints better. More angular views
alone do not admit the angular representation. High-resolution enlarged
2560×1440 EEVEE oblique/grazing views were actually opened: spatial charts retain
the shoot but blur edges; the selected angular card loses much more shape.
These renders use common unlit coverage-only emission, not a material/light
claim. They deliberately enlarge a group beyond its tested small footprint.

## Positive normal fields, separately from coverage

[Normal Filtering, SIGGRAPH2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf)
motivates filtering the response/distribution rather than only a mean direction;
PDF page3 and its equations were inspected optically. This experiment uses
the existing library's positive clamped-cosine moments, not the paper's complete
frequency-domain BRDF method. At each chart's16² field, nearest opaque source
contacts from32² samples contribute mass.25 and their original signed unit
normals in the common host frame. Four/24 bins retain mass and first moments.
No lighting is stored; query lights evaluate the unlit vectors afterwards.

Four hosts×three fixed views×12 independent unit lights are compared against
finite source cosine sums. Alpha matches the original chart exactly. All Jensen
bound controls pass. Maximum absolute response error over these cases:
one unnormalized raw first moment .38394; four bins .30202;24 bins .11103.
The diagnostic plot explicitly selects the worst first-moment field of the12,
not a newly claimed independent average. A separate scalar-loop test checks
signed light response under a rotated partition frame.

Four-bin Float64 mass/vector fields cost32768B per16² chart;24 bins196608B,
before RGB/PBR coefficients, depth, mips or upload. Positive normal moments are
not calibrated group R/T, transmission through neighbours, specular response,
shadow transport or a guarantee for another view. Charts remain separate;
packed deep-mip filtering is not introduced or admitted here.

## Sharing shoots by phase: measured counterexample

The same fixed source contains only10 exact variant-sequence families.
`crown_phase_catalog.py` groups each family by deterministic Shoot phase and
chooses an actual source host nearest each bin centre. Planning emits only
representative IDs/mapping: no poses or meshes. Original target IDs/variants
remain exact; representation identity differs from source identity.

| Phase bins | Representatives | Representative leaf rows | Max coarse vertex shift | Max corner-normal L2 difference |
|---|---:|---:|---:|---:|
| 8 | 80 | 1664 | .075915m | 1.56979 |
| 16 | 160 | 3328 | .049325m | 1.35894 |
| 32 | 319 | 6636 | .031743m | .96380 |
| 64 | 631 | 13132 | .022253m | .68132 |

The separate exhaustive verifier checks all90000 leaves per candidate. Maximum
corresponding coarse-vertex displacement also bounds matching triangle interiors
by convexity; it does not bound coverage. A conservative blade-root sphere gives
larger continuous-source position bounds .28154/.23660/.16765/.10846m.
Normal differences above are unit-vector L2 distances, not radians. Small phase
differences can still rotate leaf frames strongly. Neither phase candidate is
admitted as an LOD; do not build a whole multi-view atlas from this shortcut.

## Work, memory and verification

Frozen host comparison producer `5abf852d7`: 19.981s including432 raster comparisons,
peak105275392B. Spatial chart construction .0406–.0467s/host;64 views
.1743–.2604s/host. Raw Float64 colour/normal payload spatial278528–311296B/host,
64 views1048576B/host. Geometry/UV arrays and temporaries are additional.
A naive RGBA8 pair at the same64×16² resolution would be131072B/host before
mips or richer normal fields; this is arithmetic, not measured GPU residency.

Phase planning of all four candidates1.097s; separate exhaustive coarse/pose
verification29.947s, peak58171392B. Mapping8602B/candidate. Verifier pose-cache
array payload sum2599800B may count views/aliases; it is not unique allocation
or total traffic. No allocation/traffic total or native GPU timing is claimed.

Cold source replay at `b8181c560`: source decode/identity5.985ms, first index/refusal
64.510ms; first requested group49.620ms includes eight shared mesh/area builds,
later cold hosts1.913–2.790ms. Same-host regeneration .702–3.567ms still copies
host geometry; finished diagnostic NPZ decode2.191–3.498ms avoids generation.
These are single samples under concurrent shared-host load, not speed rankings.
The separate full-table verifier takes7.749s; process peak73613312B includes that
oracle. Each requested host holds12 array payloads,83716–87897B; total allocator
and scratch traffic are not measured. Installed replay independently agrees.

64 source and64 fresh installed-library controls pass. The wheel matches all210
current library Python files; installed PYTHONPATH excludes lib/src. No library
or native implementation changed. Ruff, format, ≤120-line functions and
`make lint-docs` pass. Linux/WSL x86_64, four CPU
affinity, Python3.13.5/NumPy2.5.4, one numeric thread, nice10. Blender5.2.2 LTS
EEVEE uses Mesa llvmpipe, not RTX3060:16 samples,33.120/21.896s render,
peak2161963008B. Producer897bc64f8 render/normal inputs are array-exact to all16
current host NPZ files; later source changes add material lineage only.

## Reproduction and remaining gap

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread and
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation. Build the saved source
using [oak_scaffold.md](oak_scaffold.md); later wood-normal changes do not affect
these host inputs. Run `spatial_cloud_probe.py --inputs <products> --output
/tmp/<fresh-controls>`, `spatial_source_replay.py --inputs <products> --controls
<controls> --output /tmp/<fresh-replay>.json`, `crown_phase_probe.py --inputs
<products> --output /tmp/<fresh-phase>`, `spatial_normal_fields.py --inputs
<controls> --output /tmp/<fresh-fields>`, `render_spatial_clouds.py --inputs
<controls> --output /tmp/<fresh-render>`, and `spatial_cloud_plots.py --controls
<controls> --fields <fields> --output /tmp/<plot>.png`.

Private originals, actual renders, reports and the minimal cold input remain
under `/tmp/outshine-vegetation-crown-lod-proof-4551504d0`; no gallery or server.
Local original Ghidra exports for GenerateBillboardLOD0x1f68360 and
GenerateOctahedralImpostor0x1f5fc80 were revisited: billboard export/material
records and angular frames are evidence of representation stages, not an
executed complete SpeedTree oracle or a coverage/light guarantee. Vendor code
and assets are not bundled. Next representation work must improve the measured
spatial footprint/normal tradeoff before a complete crown atlas. Exact-pose
shared organ/patch prototypes are a candidate to compare. Complete forest,
species form, calibrated material/R/T, wind, dynamic transitions/shadows and
native/device admission remain #23/#643/#1; the overall goal stays active.
