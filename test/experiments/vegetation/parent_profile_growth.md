# Executable parent-profile growth

[#182](https://github.com/Weichwolf/outshine/issues/182) owns the mature crown
defect; [parent_growth.md](parent_growth.md) records the area-feasible proposal.
`ParentProfileGraph` adds explicit prepared inclination profiles and bounded
world-up bias to the existing parent-length graph. Its private descriptor is
`parent-profile-generator-graph-1`. Existing graph/parent-growth descriptors and
the v2/v3/v4 plant recipes remain unchanged and reject the new version.

An inclination profile supplies the angle at the actual attachment parameter.
The existing deterministic pitch variation remains additive. Rotating the
transported frame about its first axis changes inclination without changing the
attachment or child length. Optional world-up bias normalizes
`(1 - bias) × direction + bias × world_up`; the existing normal-frame function
transports its normal. Profile bounds include pitch variation; bias is in
`[0, 0.5)`. The preparation owns its scalar values and uses immutable profiles.
Inherited experimental instance-array ownership remains as documented earlier.

Four independent checks cover effective angle against world-Z on straight
parents, orthonormal right-handed frames, unchanged attachments/lengths,
normalized upward transport, exact JSON replay and invalid/duplicate bindings.
The previous eleven parent-growth checks and original graph checks also apply.

`mature_parent_graph.py` turns a four-order tapered conifer input into executable
wood. Physical distance patterns retain complete primary whorls and distribute
secondary sites by actual parent length. Single children use a constant relative
length factor. Affine profiles extend linearly outside the occupied interval,
so their sampled lookup is exact on the interval rather than smoothing a clamp
at its endpoints. Primary inclination retains its lower-to-upper profile.

Targeted requests on the spruce 201/1 proposal produced 19/201 secondary sites
on the upper/lower primary axes. Upper lengths propagate as
0.23085 → 0.038475 → 0.017955 m at the last-parent-parameter site. Only eight
ancestral poses were needed for the two requested terminal paths.

`mature_parent_execute.py` checks the complete actual wood, physical needle
counts, window budget, finite orthonormal frames and warm queries. It stores
the explicit graph descriptor and packed wood poses; three requested paths must
replay exactly in a fresh graph without rebuilding the complete tree. Expected
physical area uses the same variant-cycle kernel as the analytic probe.
The complete frozen run on `05ea70e70` ended with exit 0 and unchanged source.
All 52,811 actual axes, 70,990 windows and order needle counts
`0 / 47,650 / 7,891,105 / 3,689,505` match the analytical proposal exactly.
Expected physical area is 203.87094066 m² against 204.14069063 m² requested,
relative error 0.132139%. The largest orthonormal-frame residual is 7.78e-16.
Fresh replay of three requested axes and their physical needle subsets is exact;
warm complete wood queries create no new poses. No complete organs were built.

The isolated protected Linux child process took 32.40 s wall time and 202.87 MiB
peak RSS. These are offline Python wood/cache observations, not frame or GPU
measurements. Files are in
`/var/tmp/outshine-vegetation-parent-execution-05ea70e70/`; terminal transcript and
getrusage record are `/tmp/outshine-parent-profile-05ea70e70.log` and
`/tmp/outshine-parent-profile-05ea70e70-cost.json`. The host lock was released and
the explicit handoff posted immediately after the terminal result.

Reproduction uses explicit private data paths:

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python -W error \
  test/experiments/vegetation/mature_parent_execute.py \
  --recipe /var/tmp/outshine-vegetation-data-2d633a079/parent-growth-area/candidate-201-1/proposal-input.json \
  --needle-source /var/tmp/outshine-vegetation-data-2d633a079/conifer-crowns \
  --target-area 204.14069063026471 \
  --output /var/tmp/outshine-vegetation-parent-execution
```

The shared host lock protects the frozen complete run. The executable graph is
stored separately from the legacy v4 analytical input; the latter never acquires
the new parent-profile semantics implicitly. Three requested physical needle
subsets replay exactly with their persisted global ID prefixes and variant cycle.

The executable form currently uses uniform source scaling and fixed bend shapes.
It does not reproduce source attachment jitter or claim full old-tree geometry.
No complete needle placement, near mesh, whole-tree atlas or botanical approval
comes from the wood ledger. In particular, very short axes also acquire thin
uniformly scaled radii; physical radius policy needs review with the actual form.
The next optical gate remains one convincing spruce before another eight-form bake.

`parent_window_product.py` binds these cached actual poses/counts to the existing
finite physical shoot catalog. It reuses the extracted internal per-axis window
packing kernel and the original catalog phase/tail binding. It must not invoke
complete needle placement/meshing or recompute any wood pose. Its complete
experimental descriptor includes both the parent-profile graph and original
physical source recipe, so the whole-product identity also includes needles.
The frozen angular bake and its optical form judgement are separate from the
completed wood ledger; the existing statistical window/LOD limits still apply.
Four independent cache/window controls check actual per-axis counts and centres
against requested needles, exact global ID prefixes/tails, catalog-capacity
refusal, transformed physical wood bounds/normals and budget refusal before
catalog I/O or window allocation. All nine legacy window columns remain
byte-exact against `05ea70e70` for a separate scalar control and all four original
conifer recipes (5,216 / 2,365 / 4,417 / 4,771 windows).

Weber/Penn SIGGRAPH 1995 §4.3 and the existing #26 prepared-profile work motivate
the parent-dependent binding. Their PDFs and the JRC spruce page were inspected
optically. These are own experimental procedures, not SpeedTree equivalence.
