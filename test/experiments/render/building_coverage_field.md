# Local projected parent coverage

Owner #1167. The same source-tree parents previously used one coverage coefficient
per XY/XZ/YZ projection and height band. This diagnostic replaces those uniform
coefficients with fixed8×8 parent-local area fields. Resolution follows the
existing8-pixel root selection heuristic; it is neither an optimal resolution nor
an image-error bound. No grid/pose sweep or new native asset format is introduced.

The compiler reads existing coarse geometry/lineage and cached BSC3 source forms.
Source XY is the footprint union including holes; XZ/YZ use the existing component
extent/height envelopes. They are explicitly not exact roof/side silhouettes.
Proxy projections are unions of the actual Float32 coarse triangles. Each cell
stores area(source ∩ proxy ∩ cell) / area(proxy ∩ cell), zero for empty cells.
Source area outside the proxy is recorded and cannot be recovered by this field.
Overlap is unioned before integration, not summed. Float32 reconstruction is
reported separately from the Double GEOS area. Each group adds768B values plus96B
Double bounds, before source hash/container overhead. This is additional residency.

Rendering uses nearest cells and the existing absolute-normal projection blend,
still at the first hit of each coarse group. It keeps full material mixtures and
the complete original remainder. Zero-opacity samples contribute the identity
transfer and skip GGX. This does not avoid rasterization, infer hidden sources,
restore depth/parallax, correlate masks, or certify caster/occluder readiness.
Loading checks the exact original node hash and never compiles fields. Source
hash reads and decoded field bytes are charged. No full source/mesh reload is
required by the field consumer; the compiler is offline asset-generation work.

The unchanged four root-boundary views compare original/uniform/local, both child
and parent cuts, at1280×720/SS16. Actual local PNGs and linear C/T arrays are saved
only to the explicit Temp output. The small cell uses the existing analytic BSC3
reference; the dense cell retains its native captured reference. Neither finite
quadrature nor analytic source geometry is a native/world/p99 qualification.

The first comparison is a mixed/negative general result: local fields reduce
the small-cell T error but increase it in the dense cell; RGB does not improve
uniformly, while GGX savings from zero cells are small. The geometry/material/
first-hit/independent-layer approximations remain. No default replacement or
native adoption follows, and finer grids are not justified by these results.
Exact costs and per-view results belong to the linked PR, not a second table here.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_coverage.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --output /tmp/outshine-coverage-fields
python -B -W error test/experiments/render/compare_building_coverage.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --fields /tmp/outshine-coverage-fields \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-coverage-view
```

SIGGRAPH-first source:
[Décoret et al.,2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
p.1/Fig.1 actually viewed and pp.1–2 read: transparent image primitives can carry
spatial coverage without detailed contours. This experiment adopts no plane-space
greedy solver, geometric-error guarantee, baked lighting or photo texture from
that work; the original parent mesh and relightable material measures remain.
