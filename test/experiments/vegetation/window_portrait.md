# Mature spruce: finite physical windows instead of an enlarged far chart

Owner: [#550](https://github.com/Weichwolf/outshine/issues/550), under #182.
This supplies an experimental closer representation of the **same** complete
cached tree used in [the lit whole-tree comparison](mature_tree_portrait.md).
It does not finish the spruce growth recipe or approve a stand/LOD/native path.

## Visible result and decision

The complete2560×1440 window portrait was opened beside #508's whole-tree
portrait. Small branch groups and the upper leader are distinctly sharper;
the far product's enlarged, soft upper crown is not an adequate close substitute.
The same narrow crown and regularly spaced upper tiers remain conspicuous.
Foliage still looks sparse and the presentation remains far below the target.
Keep the finite-window path as a closer research reference; do not replace the
cheap complete far product with it across a forest.

The local JRC *Picea abies* pp.114–115 (PDF pages opened during #508) supplies
the botanical comparison: the isolated reference crown is broader, denser and
less mechanically repeated. SIGGRAPH2003 *Billboard Clouds*, §1.1–1.2, PDFp.2,
was opened again before implementation: multiple physically anchored textured
planes permit a finer representation than one whole-object view. This is not
their optimizer or its geometric-error guarantee. Source statistical-window
coverage, conditional mean normals and partial-window tail weighting retain
their existing limitations; individual real needles are not reconstructed here.

Next: the actual wood consumes85.6% of the emitted triangles. Determine detail
before expanding thin axes, compare against this full reference, then correct
growth spacing/crown proportions. More whole-tree atlas bakes will not repair
the remaining form. Source/library/native work stays separate from Draft #202.

## What is actually generated

`window_portrait_source.py` reads the saved axes, window rows and complete finite
catalog. It verifies the whole recipe identity, executed graph/catalog hashes,
physical needle recipe, complete per-axis/count ranges, tail fractions and rigid
proper frames. It does not rerun a growth graph, needle placement or needle mesh.
The unmerged parent-profile implementation is not imported. Source coordinates
remainfloat64. These are existing experimental inputs, not a new independent
recipe-only production build or a general asset-cache implementation.

`window_portrait_geometry.py` selects a local catalog view per window for the
perspective camera, sun and fill independently. It emits physical mean-depth
quads with unchanged origins/frames, UVs, represented ID ranges and explicit
tail weights. Unlike the old whole-tree bake, nearby windows use their own
perspective rays. This and separate plane depth/material evaluation are real
representation differences; the comparison does not isolate filtering alone.

The Blender helper writes bounded arrays directly into vertices/loops/polygons
and source-normal/UV/tail attributes, avoiding a Python object per twig or a
large list of Python geometry tuples. Existing conifer materials retain their
roughness/transmission controls; a local face attribute scales the existing
alpha coverage by the partial-window fraction. Normal/material fidelity remains
experimental. No shared shader, library, native or cache API changed.

| Same complete source / new draw work | Quantity |
|---|---:|
| Tree identity prefix | `e0c6abe45a93` |
| Executed axes / finite windows | 52,811 / 70,990 |
| Represented needles | 11,628,260 |
| Expected nominal foliage area | 203.870941m² |
| Consumed source files | 13,769,659bytes |
| Decoded axis/window arrays | 19,869,354bytes |
| Decoded finite image channels | 2,359,296bytes |
| Finite family/views | 1 / 64 at32×32 |
| Packed family texture size | 320×320, twoRGBA8 + oneR8, 921,600base bytes |
| Wood vertices / triangles | 1,373,102 / 2,534,960 |
| Window triangles per role / all3roles | 141,980 / 425,940 |
| Total tree draw triangles | 2,960,900 |

All70,990windows appear in **each** role's view histogram; all64 catalog views
are used. The shader separates camera/light roles and light blocker collections.
The ground is an additional2triangles. This bounded close-reference draw is
substantially more work than the far product's384tree triangles; it is not an
automatic screen-error selector or a frame-budget result.

## Costs, exact final buffers and limits

The accepted full image uses frozen renderer `054e203c0`,32Eevee samples and the
same camera/light/flat stage as #508. It terminated0: source decode/checks0.209s,
wood assembly/binding2.723s, total population3.761s, render453.740s, measured
function457.570s, process peak3364.602MiB. Read the raw report for exact values;
the host used software OpenGL/llvmpipe LLVM19.1.7. Smaller checks/cost probes ran
during part of the render. These are offline reference timings, not GPU, frame,
native or device acceptance, and not a controlled speed comparison with #508.

One perspective selection scores4,543,360window/direction pairs. A scoped
seven-run one-role plane-emission median was0.0545s under the concurrent render;
a separate traced call peaked19,339,521bytes with28net live blocks. Net live
blocks are not allocation-event totals. All three roles explicitly write
30,667,680Blender buffer bytes; wood writes158,434,984. These exclude decoder
scratch, intermediate arrays, internal Blender copies and GPU upload.

The original wood builder held per-order double arrays and concatenated them
before Blender'sfloat32/int32 conversion. The final builder preflights counts,
allocates its final arrays once, computes transforms in double precision in
batches of at most1024axes and rounds only when assigning the final buffers. Full actual-tree
comparison with the frozen original proves **every** final vertex, triangle and
normal attribute byte-identical to Blender's previous inputs. No image rerender
is needed for that exact buffer substitution. It does not reduce triangle work.

| Scoped wood assembly | Original | Final |
|---|---:|---:|
| Returned geometry array bytes | 276,310,608 | 138,155,304 |
| Separate call traced peak bytes | 555,392,269 | 143,081,989 |
| Separate call traced live bytes | 276,318,252 | 138,162,824 |
| Traced live blocks | 143 | 140 |
| Normal3-run median, s | 1.0049 | 0.9163 |

The timings are a small sequential local comparison; no general CPU speedup is
claimed. The memory reduction is scoped to this Python/NumPy builder, not total
Blender/RSS/GPU memory. Positions differ from thefloat64 source by at most
1.907µm and normals by2.981e−8, **exactly the conversion already used** before
the change. Canonical source arrays/IDs remain unchanged.

## Verification and reproduction

Seven source and the same seven installed-wheel controls pass without skips:
independent scalar perspective/light selection, transformed corners/UVs/ranges,
tail values, rejection before budgets expand geometry, scalar tube transforms
and actual Blender vertex/index/UV/attribute readback. Physical-file controls
also accept the complete original and exact replay, reject six genuine corrupt
file cases (foreign owner, wrong tail, broken range, NaN, reflection, foreign
execution), and preserve all source windows. The final whole-tree comparison
checks all1,373,102positions,2,534,960triangles and7,604,880normal vectors.
Ruff/format, function-length checks and `make lint-docs` pass. Native gates are
not part of this Python experiment; no native integration is claimed.

```sh
export PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 LP_NUM_THREADS=1
export PYTHONPATH=lib/src:test/experiments/vegetation
proof="${TMPDIR:-/tmp}/outshine-vegetation-window-portrait-$(git rev-parse --short HEAD)"
mkdir -p "$proof"
.venv/bin/python -m unittest window_portrait_checks
.venv/bin/python test/experiments/vegetation/window_portrait_source_check.py --whole "$WHOLE_PRODUCT" --execution "$EXECUTED_AXES" --catalog "$WINDOW_CATALOG" --output "$proof/source-check.json"
.venv/bin/python test/experiments/vegetation/window_wood_compare.py --whole "$WHOLE_PRODUCT" --execution "$EXECUTED_AXES" --catalog "$WINDOW_CATALOG" --output "$proof/wood-compare.json"
.venv/bin/python test/experiments/vegetation/window_portrait_cost.py --whole "$WHOLE_PRODUCT" --execution "$EXECUTED_AXES" --catalog "$WINDOW_CATALOG" --output "$proof/cost.json"
.venv/bin/python test/experiments/vegetation/window_portrait.py --whole "$WHOLE_PRODUCT" --execution "$EXECUTED_AXES" --catalog "$WINDOW_CATALOG" --output "$proof/render"
```

The explicit input directories contain the existing complete #202 products;
they are not silently imported from another checkout. The comparison script
loads its own fixed previous Git source into a temporary module under`/tmp`.
Current raw evidence is private under
`/tmp/outshine-vegetation-window-portrait-proof-ee63756c9`; accepted image
`stable/portrait.png`, SHA256`ba827773cb1de284956095448c6ecfeb1c1180eb0536b9f6e2f09f29bf93bd21`.

The first full draw during path relocation had an incorrectly dark display and
was rejected as a matched comparison. Tiny stage controls and a fresh full draw
under stable`/tmp` paths restore the expected background142/153/161 at the
sampled pixel. Its exact original failure cause is unproven; do not move an
active Blender environment/worktree. EEVEE also reports an unavailable geometry
velocity buffer with these large meshes: object/camera velocities are its
fallback; this still frame provides no motion/velocity/wind validation.
All private data/worktrees/environments now live under`/tmp`, with dependent
environment paths repaired. No server or images under`doc/` are supplied.
