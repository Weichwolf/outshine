# Real young-pine consumer of the shared recipe pilot — #504

The existing `Conifer.plant(seed)` factory now has loadable, versioned data on
the unchanged #494/#497 experimental kernel. One actual *Pinus sylvestris* young
plant composes `PlantPlan`, three needle patterns and three physical needle
variants. Definition/instance/product identities stay separate. This supplies
Vegetation's missing real consumer for #483; its common production decision,
context index, cache contract and native port remain with master.

## Definition, form and site

Two actual definitions in `test/experiments/assets/recipes/`:
[form](../assets/recipes/vegetation-pinus-young-form-v1.json) and
[site](../assets/recipes/vegetation-pinus-young-site-v1.json). No Python callable,
pickle, seed, geography, wind phase, light or game state is embedded as a variant
per world plant. Seed is a separate UInt32 external value. The fixed young-life
stage and species are authored recipe choices, not observed source attributes.
The form reads6252file bytes, compiles5nodes; site closure7305bytes/6nodes.
These are two real recipes, not a worldwide inventory or capacity experiment.

| Exact port | Actual representation |
|---|---|
| `vegetation.seed/1` | Portable UInt32 scalar, no spatial interpretation |
| `vegetation.plant-plan/1` | Immutable finite existing PlantPlan recipe, metre dimensions, no organ buffers |
| `vegetation.needle-patterns/1` | Immutable per-order existing NeedlePattern recipes |
| `vegetation.needle-variants/1` | Immutable existing physical Needle parameter recipes |
| `vegetation.needled-plant/1` | Immutable NeedledPlant recipe; deferred geometry |
| `vegetation.needled-detail/1` | Owned immutable Float64 canonical poses/prototypes, actual integer IDs/topology |
| `vegetation.habitat-site/1` | Prepared retained StandHabitat/provenance plus owned Float64 root/rigid frame |
| `vegetation.plant-placement/1` | Unchanged form recipe and source-bound root transform/contact, no canopy certificate |

The source is the existing fixed Feldkirch input
`lib/tests/data/habitat-stand-cases.geojson`, feature2999194372, source
`openfreemap:14/8628/5746:landcover:2999194372`, region4096m, source area788.6897m².
Existing `HabitatStand.sites((6,35,42),64)` supplies canonical conifer root
`(6,35,42,1,0)` at XY`(215.2290719588,699.7388783485)`m. This procedural assignment
does not claim the source identifies a pine. Actual source geometry, effective
support, all retained attributes, frame, region, root ID, origin and orientation
enter the external fingerprint; an ID alone never keys the context.

Form metadata names root-local right/forward/up metres, datum`plant-root=0`.
Site metadata names its retained right-handed ENU frame and explicitly
`ellipsoid0-flat-study-no-DEM`. Root Z=0 is the existing flat study convention;
no height observation, DEM, neighbor reservation or geodetic certification is
invented. Source/frame binding, actual finite Float64 arrays, rigid determinant+1,
units/datum and actual prepared domain coverage are checked by the domain reader.
Changing support under the same IDs changes instance identity and can reject the
root. Its independent form product fingerprint stays identical.

`contracts.immutable`, `plan_arrays.immutable_array`, existing geometry/classes
and the common loader/registry/closure/budgets are reused. External factories
snapshot mutable callers; numeric buffers use immutable bytes backing. No second
VM, generic snapshot framework, context index or private cache is introduced.
Definitions are data; owner operators remain trusted, explicitly versioned code.
Semantic changes after publication must bump operator versions. The Python
canonical hash encoding is not an admitted cross-language/native cache ABI.

## Demand and physical checks

Request `plan`: only1node executes, no NeedledPlant table, needle pattern detail
or needle mesh is generated. Request `detail` from the form:5nodes execute,
193axes/4578needles/2289short-shoot groups and three8×8needle prototypes result.
Request `placement` from the site:5nodes execute, **no detailed geometry**.
Validate a required site first, then request/reuse the independent form detail.
The site recipe deliberately does not export detail: combined dependency
compilation order is not an implicit early-contact guarantee. This explicitly
avoids building needles before a known-invalid required root contact.

Wood orders/branches/axis count are bounded **before** PlantPlan construction
(4orders/64branches/512axes); counts and detail limits (16384needles, subdivisions
≤16×16) precede expensive generation. Common reservations cover declared product
payload bytes, not internal solver/Python/native heap/time. Worker replays apply
120CPU-seconds/3GiB address-space bounds and an external120s wall timeout;
these are diagnostic bounds, no runtime/device/frame guarantees.

`recipe_plant_detail.py` checks actual dtypes, finite buffers, complete canonical
IDs, valid supporting owners, per-axis group/per-group needle membership and
proper rigid frames. Short-shoot anchors are independently evaluated against the
analytical parent centreline/radius profile; needles must meet their actual
short-shoot tips. Maximum real anchor residual4.44e-16m. Every needle prototype
has valid nondegenerate oriented triangles/unit corner normals. This is not a
welded-junction, self-intersection, crown/terrain-collision or botanical allometry
certificate. Actual Float32 buffers under this Float64 port are rejected; a
separately specified/tested Float32 rendering representation remains possible.

Negative controls include missing exact dependency version/reference, cycle,
missing capabilities, wrong input type, reservation before callbacks, explicit
branch/detail work refusal, unslender needle-bearing twigs, actual removed habitat
support, stale/foreign frames/units/datum, actual Float32 geometry, foreign organ
owners, moved needle anchors and mutable caller arrays. Support changes preserve
form reuse; seed changes preserve definitions; changed definition bytes invalidate
dependent products. No partial checked product is published after failure.

## Evidence and measured costs

Frozen0f15e9bd6:38source and38installed-wheel controls pass (same tests, no skip).
For718/719, plan recipes, all22canonical tables and all three prototypes match
the existing factory exactly, including dtypes. Twelve fresh processes (2seeds ×
plan/detail/placement ×2) reproduce definition/instance/product/content identities.
The supplied old whole-pine rendering input is also checked: **all199products,
1816arrays with dtypes exact**, including actual wood/short-shoot meshes, needle
prototypes and poses. Existing material/scene files remain unchanged. The existing
2560×1440 four-young-conifer reference was opened after this check; it remains a
sparse young-plant reference, not a new or better forest image. No rendering,
botanical diversity, LOD, native/lib/cache/production or device gain is inferred
from an unchanged picture. Current whole-forest research remains #498/#503.

Linux x86_64/i5-1135G7, Python3.13, NumPy/SciPy/Shapely, one BLAS/OMP worker,
shared load≈12/11.6/11.5.21alternating samples after2warmups, same physical products
and reader. Direct control omits graph typing/metadata/fingerprints, so these
measure their extra cost; they do not compare equal-feature evaluators.

| Request | Direct median ms | Graph median ms | Direct/graph separate traced peak B |
|---|---:|---:|---:|
| Wood plan | 0.1614 | 1.5707 | 4720 /15946 |
| Physical detail | 264.1733 | 270.6554 | 4587061 /4613126 |

Fresh prepare5.48–9.84ms; evaluate plan1.78–2.68ms, detail327.51–392.59ms,
placement8.54–10.37ms. Process RSS high-water84.80MiB includes launch/imports;
it is not incremental operator allocation. Traced live blocks plan19/44,
detail539/816 are live counts, not cumulative allocations; native/GEOS heaps
are excluded. The proved work saving is **not evaluating unrequested detail**.

The detail owns40numeric arrays/998632payload bytes. Explicit readonly freeze
writes998632B; the two descriptor-hash passes copy1997264B via `tobytes`, excluding
other generation/checking/JSON/native temporaries. Common node emission accounting
includes final logical descriptors: plan1342B versus detail1005877B; accumulated
selected emissions1342 versus1010459B. Reservations16384 versus16842752B, plus
3seed-input bytes. Placement closure accounts8290emission bytes with98304reserved
bytes plus2394external bytes. None is a total memory or allocation claim.

## Reproduce and boundaries

With `.venv`, the existing installed reference library and NumPy/SciPy/Shapely:

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/assets:test/experiments/vegetation
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
.venv/bin/python -m unittest recipe_plant_checks verify_recipe_graph test_needled_plant test_needles -q
.venv/bin/python test/experiments/vegetation/recipe_plant_probe.py \
  --output /tmp/outshine-vegetation-recipe-<commit>-fresh \
  --baseline <existing-complete-europe-conifers/pinus-sylvestris> --runs 21
```

Installed-wheel checks omit`lib/src`; no bpy/GPU needed. Each CLI output is an
explicit system-temp path. Probe keeps original source input intact, worker
terminal logs/identities, scoped costs and the source-array comparison; no normal
test creates an oracle. Local proof/root input paths are in the PR. API remains
experimental under`test/experiments/`; no native or public library API changed.

Local first source: Weber/Penn, SIGGRAPH1995
[Realistic Trees](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf),
Fig.1/stem-child section optically opened. Existing Ghidra exports
`GetParentTemplateNode`0x209a270 and`ComputeTopologicalLeaf`0x20b79b0 were reread:
parent/target generator links and graph-terminal status are explicit. These
support hierarchy/parameter separation, not our typed DAG implementation or a
biological leaf-count oracle. No new vendor numerical run, web research, copied
SDK/VM/decompilation or foreign plant/material assets.
