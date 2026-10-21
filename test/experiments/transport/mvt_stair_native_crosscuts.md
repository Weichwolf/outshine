# Native transverse interfaces for the stair repair

The local repair need not absorb every adjoining source. It may cut an existing
corridor, provided the replacement preserves its actual interface and the same
decision reaches mesh, navigation and terrain. Four proposed cuts bound the two
paths around the original stair. Source stations select these finite observations;
they are fixture inputs, not production rules or inferred provider nodes.

| Native source part | Original-axis station | Observed native band |
|---|---:|---|
| 431 | 25 m | Approximately 2.025 m wide, almost level across the cut |
| 431 | 40 m | Approximately 2.025 m wide, almost level across the cut |
| 432 | 17 m | Approximately 2.052 m wide, about 0.335 m transverse height change |
| 432 | 48 m | Approximately 2.025 m wide, about 0.339 m transverse height change |

The first432 cut follows the original source tangent through a bend in the fitted
native geometry; its asymmetric offsets are retained. Neither width nor transverse
height is replaced by a flat1.5 m ribbon. Each profile retains all six crossing
native triangles, their page/index identities, Float32-derived ENU vertices and
computed raw intersection segments. There is no highest-surface or nearest-owner
selection. The triangle-plane intersection is linear on each original face.

`fixtures/mvt_stair_native_crosscuts.json` is a portable fixed input. Its provenance
names the cached region/manifest, original-context hash, local frame, export-tool
hash and complete private-export hash. The read-only exporter decoded all fine
road pages intersecting the stated crop:1193 manifest pages, two selected pages,
206 retained triangles without normal filtering. All 24 faces intersecting the
four finite cuts are versioned. Completeness applies to these frozen observations,
not arbitrary new queries, other world layers or the complete road network.
An independent GEOS intersection against all206 exported faces reproduces each
six-face selection. The local map and all four transverse profiles were opened.
The retained probe and tests require Python3.12 or newer.

```sh
python3 test/experiments/transport/mvt_stair_native_crosscuts.py \
  --output "${TMPDIR:-/tmp}/outshine-native-crosscuts/profiles.json"
PYTHONPATH=test/experiments/transport python3 -m unittest \
  test_mvt_stair_native_crosscuts -v
```

The inspector rejects gaps, overlapping support, vertical/coplanar ambiguity,
height discontinuities, nonfinite input and cuts too short to enclose the band.
Adjacent raw intersection endpoints must agree within1e-7 m; actual maximum
offset/height errors are reported, and raw segments are never snapped or flattened.
This is a numerical fixture inspection, not a certified exact-arithmetic predicate.

These are native mesh interfaces, not yet a replacement plan. The original source
station is not asserted to be the native navigation station. Preserve or jointly
reconstruct the navigation/terrain boundary, split the unchanged outer triangles
at the same cuts, and independently audit the joined products before admission.
The original heights inside the repair remain soft targets; a2 m experimental
prior guard is not a physical law. The actual existing432 cross profile already
differs from the sampled unconstructed design prior. Research owns the bounded
joint profile solution in #1003; Master owns native integration in #826.

Private export source, full206-face input, receipts and resulting sections remain
in the system-temp task recorded by
`/tmp/outshine-motion-integration-844/stair-ports-task.txt`. No asset was rewritten
and no new world render was needed to inspect the stored geometry.
