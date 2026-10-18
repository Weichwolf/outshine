# Shared leaf surfaces at exact crown poses: research in progress

Issue #704 follows [the cold spatial-host comparison](spatial_crown_lod.md).
The next middle-distance alternative shares eight leaf prototypes while keeping
original CrownShoots placements, IDs, variants, anchors and physical leaf area.
No leaf-pose/phase quantization, near-crown expansion, library/native interface or
cache manager is introduced. **The representation is not admitted yet:** actual
enlarged renders reveal seams in independently fitted patches. The connected
successor now has exact-pose/material lineage, a complete curved-alpha/unit-light
comparison, independent3D ray controls and an explicit raw-normal shader reader.
The original procedural colour/height/PBR comparison is now separately completed
in [connected_materials.md](connected_materials.md), including actual shader
controls/images and GGX counterexamples. Neither study admits a universal LOD;
#23/#643/#1 and the full goal remain open.

## Measured independent patches

`leaf_patch_prototype.py` partitions each original8×1 Blade's34 coarse faces:
one whole surface, two transverse halves, or four halves/longitudinal regions.
Each source face has one owner; physical area remains one value per leaf.
Existing `make_chart` fits/rasterizes each part. Folded-over projections refuse
the candidate without changing the source; a twist4-radian counterexample has a
valid source but refuses one card and accepts four patches.

`LeafPatchCatalog.group` checks capacity before poses/meshes and requests only
the original20/21-leaf host. At most one pose group is retained per independent
catalog. It builds only eight shared coarse Blade prototypes,272 source triangles
total. A group holds original read-only poses/IDs/variants, physical area and
material lineage; diagnostic chart emission is separate. Reordered requests and
direct original-anchor transforms agree. Renderer inputs store finished arrays.

Four actual original hosts17/2178/3925/5471 contain83 leaves.24 independent
spherical directions include grazing views;4²/8²/16² footprints use the same
viewport across all four methods.1152 cases compare sum absolute coverage
difference / sum maximum coverage. Controls remain identical in content.

| Footprint | Old spatial mean / max | One shared card | Two shared patches | Four shared patches |
|---|---:|---:|---:|---:|
| 4×4 | .06160 / .13670 | .04873 / .11232 | .05859 / .14953 | .03784 / .09089 |
| 8×8 | .06077 / .15204 | .04328 / .08981 | .05278 / .10251 | .03346 / .05870 |
| 16×16 | .07598 / .14006 | .04799 / .10357 | .06936 / .10881 | .03460 / .05924 |

Reference680–714 triangles/host; old spatial34–38; shared1/2/4 surfaces
40–42/80–84/160–168. More geometry buys smaller measured coverage error while
sharing eliminates per-host image storage. Four-patch images for all eight
variants occupy524288B Float64 colour/normal arrays plus10496B mesh arrays and
256B normal bounds. These are unique template payloads, before mips and native
packing; host pose/area payloads2820–2961B exclude containers/allocator overhead.

The completed2560×1440,16-sample EEVEE oblique/grazing images were actually opened.
Left reference / middle old spatial / right shared4-patch: small silhouettes are
better preserved, but internal fine transparent seams are plainly visible in the
enlargement. Independent fits and filtering independently cut alpha support.
Do not call this near-LOD quality or publish the prototype as a final species.
Two patches are not consistently better than one; retain that counterexample.

## Normal data without redundant retained fields

Original signed normals and coverage mass form unlit raw first moments. Initially
four positive bins required an extra1048576B for the32 shared patch images.
For this source, four-patch worst same-chart unit-cosine response error was
.0187001 with one raw moment and with four bins; both worst finite Jensen bounds
were .0262731. Sparse bins did not justify that additional retained payload.

The existing premultiplied normal RGBA now stores the moment itself:
mass=a, local vector=2rgb−a, original blade vector=local vector×frameᵀ.
**Do not normalize the decoded vector.** Its length carries variation, not only
direction. Per-chart scalar bounds remain. Source-contact construction normalizes
individual Float64 samples, then filters vectors/mass; no baked light is saved.
Independent scalar signed-contact checks and an explicit vector-length control
cover this decoding. All56 patch fields,24 independent lights each, are separately
checked against original finite source contacts; these are not group transport.
View-dependent coverage, material coefficients, specular response, neighbour
shadows and calibrated RGB/PBR/R/T remain separate requirements. Original
procedural material inputs remain bound; their qualitative shader is not upgraded
to a botanical optical oracle by these fields.

## Avoiding irrelevant work in the coverage verifier

`patch_coverage.py` streams alpha union1−∏(1−alpha). Opaque coverage does not
need normal-map decoding, lighting, depth sorting or all-chart raster stacks.
Source coverage and shared-diagonal controls remain. All1152 full-cohort coverage
metrics match the old shaded verifier within2.58908e-16 after compacting fields.
The old `cloud_experiment.view` interface is unchanged.

Frozen initial producer6a79a80e2:187.277s cohort comparison,251838464B process peak.
Compact/stream producer5c6d69e14:54.581s,64905216B. These change both verifier
work and retained moments; do not attribute an isolated engine speed factor.
A separate same-current-chart128² probe,84 charts/1376256 chart-pixel slots:
legacy/stream median of five untraced calls .8421/.2135s, traced scratch peak
177771870/2136581B, process peak245948416/58138624B. Stream sorts zero slots.
Positive live allocation differences41/36 are live snapshot counts, not total
allocation count or cumulative memory traffic. Memory tracing uses a separate
call; those timings are not mixed. Shared-host llvmpipe previews were concurrent;
these are verifier measurements, not native/GPU/runtime acceptance.

First actual group builds1/2/4 policies .0646/.0705/.1063s including all eight
template/area builds; later hosts approximately .0008–.0013s with templates
reused. Each method owns an independent source plan, so candidate timings do not
borrow the reference's already-created poses. No complete90k-pose or crown-mesh
build is used by this cohort.

## Connected carrier and finite source bounds

`connected_leaf.py` replaces independent cut textures with a continuous unrolled
leaf domain(x=s×width(t), y=t×length). One whole-leaf alpha/normal image is shared
by the entire carrier. A3×3 vertex grid has8 triangles, shared internal edges and
the exact original petiole attachment. Source coarse halves are linear in the
transverse coordinate; carrier frames come from the existing analytic arc frame.
Normals in this map are raw moments in the **blade object frame**, not a tangent
normal map for an arbitrary quad. That encoding requires an explicit consumer.

For each original/carrier UV triangle pair, Shapely intersects their domains.
The norm of the difference of two affine3D maps is convex on each overlap;
therefore its maximum is attained at overlap vertices. Coverage of the original
UV domain and area multiplicity are checked before reporting the maximum.
Tests include the opposite-diagonal case: identical corner positions still have
an interior .5 displacement; a vertex-only test would incorrectly report zero.
A duplicated half and uncovered half cannot cancel in the area check.

Eight actual source variants at32²/8 carrier triangles have computed paired
coarse-source bounds .000714–.003454m. Their finite native raw-normal Jensen bounds
are .009154–.021565. These are Float64 computations, not IEEE-certified global
bounds, analytic full-Blade bounds, alpha coverage or complete optical transport.
The original UV domain is retained; the carrier covers it and masks the exterior.
It does not alter the saved source or placements. `ConnectedCatalog` now binds
the original source catalog, material JSON, pose plan, organ IDs/variants/anchors
and physical area. Capacity is checked before work; only demanded variants and
one host pose group are retained. Eight shared prototypes require272 coarse source
faces, not a complete90k-pose/near-crown product. No common/native API is added.

The finished-array reader now also renders the connected candidate without
source pose/prototype generation. It checks original IDs/variants/areas and uses
the original stored poses. Frozen reader4e03fd1dc holds the previous camera basis,
directions and2.198960224m span;714/38/168 triangle counts are unchanged.
Both2560×1440 oblique/grazing images were actually opened. The earlier fine grid
seams are absent in these views; the connected result resembles the reference
more closely, with remaining edge softness. This is two coverage-only views of
one host, not botanical quality, lighting or complete LOD admission. Original
seam counterexamples are retained. The separate newer comparison follows below.

In particular, a curved alpha mesh cannot simply use the planar chart verifier's
nearest geometric triangle: a transparent front sample may reveal another part.
Correct fragment handling and shared-edge ownership must precede its comparison.
Continuous UVs remove the known independent-cut mechanism in the inspected
views; do not generalize this to every view or material.

## Complete curved-alpha and unit-light comparison

Frozen producer7af28c021 recomputes every method with the same actual four hosts,
83 original leaves,24 spherical views and4×4/8×8/16×16 footprints,8× supersampling
and six independent unit-light directions.1152 comparisons use **raw generated
linear Float64 image arrays** for all methods. The earlier table uses rounded
8-bit alpha and a different planar verifier; do not splice numbers between them.
The six lights produce both positive and negative cosine responses,12 fields.
Coverage error is L1 divided by per-pixel maximum coverage sum; response uses
the same denominator. The response error includes coverage/depth/occlusion
differences; it is not just the same-chart normal approximation/Jensen error.

| Footprint | Old spatial coverage mean / max | Independent4 coverage | Connected16 coverage | Connected32 coverage |
|---|---:|---:|---:|---:|
| 4×4 | .06098 / .13212 | .03670 / .08473 | .04947 / .09132 | .03667 / .09128 |
| 8×8 | .05984 / .15653 | .03394 / .06330 | .04503 / .09392 | .03375 / .09074 |
| 16×16 | .07608 / .14737 | .03464 / .05925 | .04815 / .07168 | .03596 / .06014 |

At8×8, the corresponding12-field response mean / maximum is
.01847/.11732, .01038/.04211, .01353/.07125 and .01039/.07338.
Connected32 removes the inspected seam mechanism with similar average error to
independent4 at the same160–168 triangles/host and524288B unique Float64 image
payload. Its worst views are worse: **not a uniform dominance or LOD admission**.
Connected mesh arrays8256B replace10496B independent mesh arrays; physical leaf
area is counted once. Connected16 uses131072B images but loses more coverage.
Neither policy changes the actual original poses or variants to improve a view.

`surface_fragments.py` retains all curved geometry layers until UV alpha has
been sampled, then sorts by depth for Porter–Duff over. A transparent front face
must reveal the back, which the old nearest-geometry path fails explicitly.
Shared edges are owned once by topology plus original uint64 organ ID, without
XYZ welding. Separate coincident organs survive; stable source/face tie keys
remove packet/face-order dependence. Fragment capacity is explicit1048576.
The sampling boundary rule remains the existing raster1e-10 barycentric policy
and1e-12 pixel-squared projected determinant cutoff. It is not an exact-predicate
global geometry rule. Seven analytic controls include scalar depth composition,
shared diagonals, separate uint64 extremes, raw-vector length/rotation and refusal.

`connected_ray_check.py` reads finished arrays only. An independent3D Float64
edge/ray linear solve checks four spherical directions and512 fixed64×64 pixel
centres on every actual leaf and each source host:348 cases,8022 identical
triangle/pixel hits, maximum depth difference3.28e-15m. Boundary pixels within
1e-8 barycentric distance are explicitly reported; this sample encountered zero.
Condition numbers reach2.06e6. This is a finite interior-ray oracle, not a global
IEEE certificate; shared-boundary correctness has separate analytic controls.

The bounded cohort finishes in57.567s with73781248B Linux process RSS peak.
First connected16/32 group builds including eight templates/area take
.1764/.2470s; the three later groups .0010–.0027/.0013–.0057s. Different actual
hosts and shared-host load preclude a speed ranking. The study retains four
groups for comparison only; each catalog retains one pose group. Largest
connected32 candidate fragment packet13047, old spatial23727. Arrays/work counts
are explicit; cumulative allocation/traffic and native/GPU/device costs remain
unmeasured. These measurements do not admit a whole tree or forest fallback.

## Actual signed-normal shader consumer

`connected_reader.py` loads finished NPZ products and original saved poses,
checking original IDs/variants/physical areas before placing shared prototypes.
`connected_normal_material.py` reads source CORNER vector attributes explicitly,
normalizing individual interpolated source normals without Geometry.Normal's
backface change. Candidate Float32 channel-packed linear images decode
`m = 2rgb - alpha`; the unit light is transformed into the original blade frame.
The shader never normalizes m: emission `max(m·light,0)/alpha`, mixed with
transparency by alpha, implements the premultiplied unit response. This is not
an ordinary tangent-normal map and not a calibrated material or R/T response.

Four actual backside-plane shader calibrations check source±light=1/0 and a raw
moment length.25 with mass.5 giving .25/0. A wrongly normalized mean would give
.5. Maximum actual central-region error is.000787, below the declared.02
tolerance, including EEVEE dithering
and PNG quantization. Node checks reject Geometry.Normal and normalization of
the raw candidate. No image is illumination-baked.

The host reader uses714 original versus168 candidate triangles per subject.
Two opposite diagnostic lights and two enlarged2560×1440 views exercise the
unchanged saved host. Frozen reader2b5fed45b renders32-sample oblique/grazing
views in20.406/21.211s with2151927808B process RSS peak. Both PNGs were actually
opened: matching signed bright/dark regions, no earlier fine internal grid seams,
but softer silhouette edges and remaining reduced-surface differences. Side
light ±(.927229,−.347711,.139084) deliberately exercises both signed responses;
no actual whole-crown material/performance claim follows from these enlargements.
Original procedural fern colour/height inputs stay bound
in catalog identity but are deliberately not evaluated by this unit-light probe.
Their faithful mapping and the actual original qualitative BSDF comparison are
now documented in [connected_materials.md](connected_materials.md). No ordinary
leaf shader/material family is replaced by this unit-light consumer.

## Sources, checks and reproduction

Local SIGGRAPH2003 Billboard Clouds, page4/figures/greedy procedure, was inspected
optically. This uses its representation principle, not its full optimizer.
Normal distribution reasoning follows the local2007 Normal Filtering source
and existing library finite-measure bounds; it is not a full BRDF implementation.
Local2011 TVCG Stochastic Transparency page2/equations/figures was also opened
optically: sorted over/product transmittance grounds the CPU reference; the
paper's stochastic GPU method is not implemented or claimed.
Ghidra was actually rerun read-only on the original SpeedTree10.2 binary:
SHA2569bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c,
ComputeLodSkin0x1b3f900, terminal status true. Stored SLod/ComputeLeafMesh stages
are visible statically; this does not establish our partition strategy, a live
complete Modeler oracle or any coverage/light guarantee. No vendor code/assets
are bundled. Ghidra settings/cache/logs and every private result use/tmp.

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread,
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation. Run
`leaf_patch_probe.py --inputs <cold-source> --output /tmp/<fresh-study>`,
`render_leaf_patches.py --inputs <study> --output /tmp/<fresh-render>`,
`patch_coverage_cost.py --inputs <cold-source> --output /tmp/<cost>.json --method
legacy` or `stream`. The old saved prototype arrays can be compared using
`render_leaf_patches.py --inputs <study> --connected
<connected-prototypes> --output /tmp/<fresh-connected-render>`; each
`prototype-N.npz` contains mesh arrays plus colour/normal from build_connected.
Current comparison: `connected_comparison.py --inputs <cold-source> --output
/tmp/<new-study>`, then `connected_ray_check.py --inputs /tmp/<new-study>
--output /tmp/<rays>.json` and `render_connected_normals.py --inputs
/tmp/<new-study> --output /tmp/<fresh-normal-render>`, LIBGL_ALWAYS_SOFTWARE=1.

Private inputs/evidence are under
`/tmp/outshine-vegetation-leaf-patches-proof-874fa2f6b`; the six-file cold source
remains `/tmp/outshine-vegetation-crown-lod-proof-4551504d0/final-replay`.
Current curved/light evidence is under
`/tmp/outshine-vegetation-connected-study-68300d598`. A fresh isolated wheel
includes master #708's geometry update and matches all210 focused-source Python
library files.62 focused-source and62 freshly installed checks pass: leaf_patch,
patch_coverage, connected_leaf, connected_catalog, surface_fragment,
spatial_source, spatial, leaf_normal, prototype_normal, crown_shoot, raster_rows,
normal_lobe and normal_tile_io `_checks` modules. The earlier77-test stage is a
different recorded subset, not added to the current count. Permanent Draft202
library changes remain outside this focused delivery and wheel identity claim.
Ruff/format/function-length and lint-docs pass; the actual renderer is checked
by its terminal completion, original-ID/area assertions, camera/count checks and
opened PNGs rather than by those CPU unit tests.
No library/native implementation or gallery changes. EEVEE5.2.2 LTS uses forced
software rendering; independent10.915/11.879s,2143604736B peak; connected
21.744/15.655s,2132733952B peak. Shared-host preview timings are not speed rankings.
The later original-material comparison and current controls live in
[connected_materials.md](connected_materials.md). Complete angular/mip/wind/shadow
behaviour, crown/forest, species and native/device validation remain open;
overall goal stays active.
