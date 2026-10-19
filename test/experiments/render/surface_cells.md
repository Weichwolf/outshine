# Complete known surface-cell occupancy costs (#885 / #844)

This cost reference replaces the older **observed ray-hit count** with explicit
triangle/cube incidences for the entire supplied native group. It builds neither
a voxel renderer nor a material approximation. Source/Part/Face identities remain
in the immutable original table, addressed by each stored original triangle index.
No unseen source is inferred from these two existing native inputs.

## Construction and numerical scope

`surface_cells.assemble` takes triangles in an explicit caller-owned metric frame.
The fixed grid origin encloses the whole input; no eye or orientation selects it.
Closed boundary contacts include both adjacent cells. An AABB proposal precedes
13 separating axes. Each face is atomic for candidate/incidence limits; rejected
faces remain original fallback and affordable unrelated faces continue. Degenerate
faces remain fallback too. Cell coordinates and original-face CSR are explicit.

The reference supports 131072 faces, coordinates within +/-1e9 m, pitch >=1e-6 m,
65533-cell span per axis, 131072 candidate boxes per face, 4194304 globally and
524288 retained incidences. Batches contain at most 32768 candidates. Large arrays
follow budget checks. These are experiment domains, not native quality profiles.

SAT uses Float64 with a scale-aware 64-epsilon allowance. This is not interval
certification or a global no-false-negative proof. Independent six-plane polygon
clipping uses exact rational interpretations of the binary grid inputs. Seeded
synthetic cases include contacts; each actual corpus additionally checks 128
accepted/proposed incidences. Whole-face disposition and CSR ownership are checked.

A touched cube, its volume/boundary, visible opacity and a relightable material
are distinct. A cube diagonal is not a rendered coverage/depth error certificate.
There is no source admission, invented capture radius, native codec or GPU binding.

## Original whole-group observations

The versioned `native-cell.json.gz` is unchanged, SHA256
`e29429e2fa670c2a5ce4b2357ee274c31fc6be7764866737b57933fb863a6c99`.
It supplies 131 sources with complete Full/Fine controls. Full is not relabelled
as the requested pure-Shell Tokyo view. Both runs use the original parent-relative
ECEF axes and exactly 1 m pitch; this grid orientation is not optimised.

| Whole known group | Full | Fine |
| --- | ---: | ---: |
| Original triangles | 2951 | 18081 |
| Candidate boxes | 696733 | 944066 |
| Axis/box projection pairs | 9057529 | 12272858 |
| Triangle/cell incidences | 100641 | 169448 |
| Distinct occupied cells | 47862 | 48128 |
| Original fallback faces | 0 | 0 |
| Actual coordinate/CSR/output array bytes | 884171 | 1177189 |
| Declared14B-per-cell payload bytes | 670068 | 673792 |
| Declared original48B vertices + UInt32 indices | 342516 | 1951692 |

Each actual group passes 128 rational clipping controls with zero omitted or extra
contacts. Tests also cover independent dyadic cases, exact plane-boundary sides,
whole original membership, unrelated local fallback and hard/empty domains.
The 1280×720 diagnostic projects **occupancy densities**, not visibility or a
runtime image; both figures were opened. Fine increases distinct cells only
slightly here; that does not establish equivalent material/appearance quality.

The 14 B model includes6 coordinate,4 normal and4 colour bytes; those latter fields
are not constructed. It excludes opacity, PBR, hierarchy/mips, memberships and
fallback. It is neither native GPU layout nor a global encoding lower bound.
Actual complete original-face CSR costs 594016/870308 B separately. Empty-space
removal alone cannot make this **fixed 1 m model** beat the existing Full geometry.
No renderer is built on that cost result. Adaptive/visibility-aware material
models remain alternatives, not disproved by uniform geometric occupancy.
The requested actual dense Tokyo view and native/world/device evidence remain open.

## Reproduction and measured work

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -W error \
  -m unittest test_surface_cells
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -W error \
  test/experiments/render/measure_surface_cells.py --representation Full \
  --output "${TMPDIR:-/tmp}/outshine-research-complete-cells-full"
```

Use a fresh output; replace Full with Fine for the second input. NumPy and existing
native JSON/mesh readers plus Matplotlib are reused. No library/native changes.
NPZ stores all cells/CSR/fallback; JSON records hashes, calls, known array payloads,
traced peak and Linux RSS. Coordinate-construction/concatenation/sort arrays are
counted; total allocations/copies/physical bus traffic remain unmeasured. Wall/CPU
covers assembly only; controls, input preparation and plotting remain outside it.
The observed non-isolated source runs took 7.38/42.81 s, traced 8.34/16.95 MB and
process RSS 201.47/211.28 MB; these are Python observations, not native budgets.

SIGGRAPH-first: [Far Voxels, Gobbetti/Marton 2005](https://www.crs4.it/vic/data/papers/sig2005-farvox.pdf),
p.4/Fig.4, sampling and shader fitting textually/optically inspected. The paper
uses visibility-aware directional appearance, which raw occupancy does not supply.
[Triangle/box SAT, Akenine-Möller 2001](https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/pubs/tribox.pdf),
p.2/Fig.1/13 axes textually/optically inspected. This is an own NumPy implementation;
no author code is copied. Exact polygon clipping is a separate control algorithm.
