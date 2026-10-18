# Automatic joint seeds and the regional precision countercase

Research: #713, free junction levels #714, actual regional precision #717.
**No wider world product is admitted by this experiment.** The verified
87-source reference from [PR #711](https://github.com/Weichwolf/outshine/pull/711)
remains unchanged. The independently completed profile-interval correction is
[PR #719](https://github.com/Weichwolf/outshine/pull/719), merge
`2480e182ee28e1f80cbbf31d3f7433997d9afbef`; #718 is closed.

## Original input and bounded selection

Use the same original Wien `14/8937/5678` transportation payload,
`test/outshine/data/transport/wien-junction-14-8937-5678.pbf`, SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
The unchanged 84-source resident baseline and its complete original DEM/source
inputs are retained with the preceding [joint-crossing report](stair_joint_crossing_comparison.md).
Original source IDs, tags, axes and repeated ring-closing node occurrences survive.
The MVT graph is not exact OSM/legal routing.

`stair_growth_experiment.py --family seed --seed-root-form closed
--seed-crossings walk` augments original-node proposals before fitting. The
legacy `--seed-crossings none` remains the default and reproduces the previous
selection. Both modes keep whole original source parts. The new policy follows
ordinary ground road/footway and footway/footway crossings only; public walk in
both directions is required. Shared original nodes use the original contract.
Private/conditional access, wrong levels, repeated/self-intersecting axes and
multiple intersections do not become geometric shortcuts. No motor/cycle turn
onto a footway is invented.

The installed original-node planner is unchanged. The experimental dependency
stage uses at most 20,000 records/200,000 node occurrences, 32 recipes, eight
whole sources/four geometric contacts per recipe, 2,000,000 candidate visits,
65,536 axis tests and 64 crossed-pair records per recipe. A bound produces an
explicit deferred recipe, never a falsely complete closure. Actual geometry
attempts remain at most three per call, with explicit bounded continuation.
Dispositions include dependency sources, not just the original graph proposal.

The first remaining whole group contains these suffixes of the original IDs:

| Source | Whole length, m |
|---|---:|
| `1574593312:0` road ring | 145.3437303124 |
| `103612620:13` approach | 254.6968813999 |
| `103612620:16` short road | 8.3787874262 |
| `39881120:19` cycleway | 6.2163373869 |
| `80922970:254` footway | 74.5391590064 |
| `80922970:246` footway ring | 180.5785581509 |

Total 669.7534536827 m, four procedural walking crossings. The short parts do not
replace the whole 254.7-m approach. The old manually planned three-source group
is contained in this selection, but its geometry would be replanned from 84,
not appended to frozen 87-source contacts. The canonical closing ring node has
two real end profiles with the same DEM sample; other repeated nodes remain errors.

## Equal-result selection costs

Frozen `ce1794545f07aff5d28e1a7280902b358ca89dde`; both indexed and scanned
selection produce the same roots, recipes, reasons and dependency evidence.
Recipe SHA256: `4c570e592aec46787c3812b497e424fab27295fc166003167f67c223e39ea669`.
Dependency work summed over all seven original recipe closures:

| Measurement | STRtree | Full candidate scan |
|---|---:|---:|
| Source queries | 31 | 31 |
| Candidate visits | 210 | 7,767 |
| Axis tests | 134 | 7,506 |
| One unprofiled selection, s | 0.24965 | 0.26389 |
| Profiled selection, s | 0.72253 | 0.92430 |
| Memray `total_num_allocations` | 1,139,569 | 1,313,981 |
| Cumulative requested bytes | 104,246,023 | 111,970,683 |
| Tracked incremental peak, bytes | 1,391,848 | 1,265,960 |

The index reduces measured work and cumulative allocation, but raises the tracked
peak. One noisy time sample is not a speed or device-budget acceptance.
Loading/verification, source decoding and geometry fingerprints are outside
these timers/traces. Copy traffic and library-internal temporary work are not
fully counted. Hardware remains the shared WSL2 i5-1135G7/4-logical-CPU host,
not A18 Pro or RTX 3060; other agents/processes were active. Python 3.13.5,
NumPy 2.2.4, SciPy 1.18.1, Clarabel 0.11.1, Shapely 2.2.0/GEOS 3.14.1,
Memray 1.20.0; BLAS/OMP threads one. Measurements use normal priority.

## Height alternatives and actual refusals

Independent DEM-derived junction means are procedural priors, not supplied
physical pins. Free exact flat-region equalities remove the real infeasible
fixed-prior fit. The continuous alternative first fits shared levels, then pins
their exact Float32 datums under a shared eight-second/100-conic-iteration budget.
Its real 520-vertex/765-face road surface passes Double but fails absolute
Float32 on one very thin face. The captured whole-face countercase is
`test/outshine/data/transport/joint-ground-height-quantization.json`.
No increase of source crossfall, grade, source grid or physical contact guards
is used to admit that failure.

The other alternative determines integer height steps in one normal Float32
binade. Sixteen halfspaces with bound `cos(pi/16)` form a conservative polygon
inside each unchanged normalized grade disk. Exact flat-region equalities stay
linear. The finite height domain is derived from the same priors. Unsupported
binades give explicit absence. Limits are 4,096 vertices/8,192 faces, eight solver
seconds and 100 MILP nodes. Nodes are **not** conic iterations. Solver termination
alone never admits a mesh; actual whole D/Q grades, exact representation and
flat groups decide. A feasible candidate returned at the HiGHS solution limit
is not an optimality proof. This expensive method is research, not a runtime proposal.
Joint-height allocation/copy counts, MILP peak memory and actual device encoding
costs remain unmeasured; the limits and isolated feasible result are not a full
cost acceptance. Profile these only if the path remains useful after #717.

The standalone quantized six-source road body passes the full physical audit
after the independently corrected profile-interval comparison. The larger
whole local generator still refuses it: new terrain has one Float32-inverted
facet. In the final three-attempt run, two candidates reach this terrain refusal;
the other has no usable quantized candidate under its node bound. No new assets,
consumer products or native admission are published. An earlier minor/foot-only
closure admitted four sources but omitted further walking dependencies; it is
retained as a historical trial, not substituted for the broader target.

## Same geometry, explicit regional encoding

The complete fixed terrain counterexample is only 15,919 bytes:
`test/outshine/data/transport/regional-terrain-topology.npz`, SHA256
`eb5d2ee325f503fea371627a294ee414a98ca802e046750f0a046fbdb46ad4ea`.
It contains the actual 1,002 positions/1,896 faces, not a hand-picked replacement
triangle. Companion JSON retains the original bad facet and producer.

`stair_regional_precision.py` subtracts an explicit origin in Double before
storing Float32; diagnostic reconstruction is separately identified. With the
same whole patch and regional origin `(-704,320)` m, all orientations and planar
coverage pass, without merged vertices. The offending signed double area changes
from Double `+0.00085648835845` m² / absolute-Q `-0.00059399008751` m² to positive
regional Q. Maximum vertex error falls from 32.487870 µm to 7.827490 µm.
The 8,016-byte position payload is unchanged; the 2D origin costs 16 bytes.

The same continuous road fit, with origin `(-704,320,160)` m, has zero violating
faces and maximum reserved normalized grade 0.8741088056 instead of absolute-Q
2.2733208050. Position payload stays 6,240 bytes; the 3D origin is 24 bytes.
Maximum vertex error is 0.227253 µm. This does not alter logical Double inputs.
It suggests that the costly integer workaround can be avoided by the appropriate
representation, but does not prove mixed-region contacts, conservative occupancy,
terrain heights, actual GPU transforms or a complete wider world. Those remain #717/#568.
The current absolute-Q gates have not been silently redefined.

## Verification, visual limits and reproduction

Research controls on `31f8a1305`: 285 source +285 installed controls pass with all
122 actual library imports matching their declared roots. Two additional regional
controls pass in each mode. The unchanged 212-file wheel is the PR #711 wheel;
no library source or dependency changes. The independently delivered trace fix
has its own 271+271 controls and fresh actual 87-source readers on its frozen
topic commit, detailed in PR #719. Original accepted verification receipts are
never overwritten; later readers use fresh hardlinked copies.

The regional whole-input check is directly reproducible:

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=lib/src:test/experiments/transport \
  python3 test/experiments/transport/stair_regional_precision.py \
  --input test/outshine/data/transport/regional-terrain-topology.npz \
  --output /tmp/outshine-transport-regional-replay
```

Current private traces, complete original trials, reader receipts and compact
measurements remain under `/tmp/outshine-transport-autojoint-5e04d0de4/`.
There is no new accepted image improvement: the existing complete private 720p
87-source reference images remain the visual baseline. No new showcase/gallery
or public image is introduced. A wider world still needs actual whole 3D images
and actual Buildings/Vegetation readers after the precision contract is resolved.

SIGGRAPH first: Chen2008 [page 2/Figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
optically checked in the preceding study, supports separating graph planning
from geometric construction. The established own cone matrices are reused;
[Clarabel](https://clarabel.org/stable/python/getting_started_py/) and
[SciPy/HiGHS MILP](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.milp.html)
document the solver formulations and different limits. No third-party code or
SDK geometry is copied. General optimality, precision and runtime/world/device
acceptance remain unproven. #434/#713/#714/#717 and the full goal remain active.
