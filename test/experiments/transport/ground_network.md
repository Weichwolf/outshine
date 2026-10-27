# Connected small ground components

Owner [#1527](https://github.com/Weichwolf/outshine/issues/1527), parent
[#1402](https://github.com/Weichwolf/outshine/issues/1402). Python reference only.

The existing explicit three-arm recipe handles exactly three spans. The new
`outshine/coarse-ground-tree/1` choice uses the same field, source-scope and terminal
producers for a complete connected source tree of three through eight spans,
maximum node degree six. Existing chain and three-arm defaults retain their
recipes and outputs. No runtime format, native renderer or second transport
pipeline is introduced.

Every internal node has one convex full-profile core. Edge corridors share its
exact whole cuts; degree-two bends receive their own core. Long corridors have
proximal planar bands, while leaves have finite terminal bands. Arm bands keep
the source longitudinal grade and cross-grade separately; only actual junction
cores use an isotropic minimum. Source widths, IDs, frames and ownership remain
explicit. Cycles, overlapping unrelated arms, insufficient length, unsupported
levels/profiles, missing inputs and exhausted budgets are local failures.

At degree-two connectors the cut constructor first checks the existing
closed-form common-distance candidate against every whole-profile inequality
and bound. Its local bounds/direction failure invokes the existing minimum-sum
HiGHS cut solve. Branching nodes retain that LP directly: a common cut can obey
local inequalities while consuming too much of the actual short side arm for
the subsequent C1 field to terminate regularly.
Malformed input or expired time does not select a fallback. This avoids linear
solves where a directly checked construction suffices. It does not claim a global
optimal road layout or minimum cuts for the fast candidate. Independent union
coverage, complete shared edges, field/finite grades and original DEM errors
remain required. A common-only control and a linear-only control are experimental
unbound alternatives, never silently published under the recipe identity.

## Actual source limits

The fixed five-span fixture produces a connected finite surface with two inner
bends, one three-way core and ten shared whole sections. The existing strict
source-scope entry still rejects it before DEM: `MissingTopology` is only mapped
to an entire feature. Its generated outer sections also lack qualified original
end identity. The separate diagnostic constructs and audits geometry without
claiming source admission or metric external contacts; these reasons and logical
Unknown are preserved. It reports zero qualified World joins.

The same recipe also runs through the shared source/terminal producer on the
existing qualified three-arm control, including its short arm and mixed widths.
There branching-node LP selection preserves the accepted field; the common-only
control is unsuitable for the subsequent C1 solve despite feasible local cuts.
No prior/grade/contact limit is enlarged to accept either control.

The precise omission/Way/influence contract is owned by the decision in
[#1402](https://github.com/Weichwolf/outshine/issues/1402#issuecomment-6448913011).
Geometry readiness does not remove that missing source authority. Terrain
integration, whole bridges, routes/vehicle envelopes, World integration and
native device budgets remain separate requirements.

## Reproduction

Use the checkout's `.venv` or equivalent with NumPy, SciPy/HiGHS, Shapely/GEOS,
Clarabel and Matplotlib; allocation measurements additionally require Memray. Outputs must be
fresh directories under the system temporary directory.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
.venv/bin/python -B -W error -m unittest \
  test_canonical_ports test_canonical_bridge_ports test_ground_network \
  ground_star_checks ground_terminal_checks ground_network_checks -q
.venv/bin/python -B -W error test/experiments/transport/replay_ground_network.py \
  --output <fresh-system-temp-geometry>
.venv/bin/python -B -W error test/experiments/transport/measure_ground_network.py \
  --output <fresh-system-temp-costs>
```

The diagnostic saves raw original/fitted geometry and a view at actual metric
aspect; no height exaggeration or publication. Cost samples alternate three warm
orders on the same five bound spans and require the same quality limits. They
include geometry, DEM observations, field fitting and finite audit. Decoding,
index creation and source admission are outside this isolated stage. Allocator
traces are separate; payload bytes are neither total copied bytes nor app peaks.
Full-worker peak and process priority are reported. Actual values, opened images,
commit-qualified terminal results and the selected-method decision belong in the
PR; no device/frame/World admission follows from Python measurements.

## Sources

SIGGRAPH2008, *Interactive Procedural Street Modeling*, page2/Figure2 separates
street graph and generated geometry. Galin et al.2010, *Procedural Generation of
Roads*, page8/Figures17–18 uses explicit width sections and separates surface
roads, bridges and tunnels. Both local PDF pages were inspected visually for this
step. This bounded core/corridor construction is an extension of Outshine's
existing port recipe, not a reproduction of either complete paper pipeline.
