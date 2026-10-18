# Retessellating free terrain for whole automatic transport seeds

Research #713/#714/#725; broader regional representation #717 remains separate.
Commit `29ff36db2f2f63220be614f3ae6dcb53efd2cbb7` produces and independently
verifies the whole six-source, four-walking-contact candidate from the unchanged
84-source baseline: 90 whole original line parts, 669.7534536827 m in the new
module. The earlier delivered 87-source product and receipts remain unchanged.
This is a bounded Python result in Draft PR #720, not native/world admission.

## Inputs and construction

Original MVT, DEM, source IDs, lengths and baseline hashes are documented in the
[joint-seed trials](stair_seed_dependencies_comparison.md) and
[delivered 87-source comparison](stair_joint_crossing_comparison.md).
Same named Wien ENU frame and original 84-source baseline; both rings, the entire
254.6968814-m approach, short road/cycleway and connecting footway are retained.
Original nodes remain distinct from four explicitly procedural walking contacts.
No motor/cycle turns onto footways, clipped substitute group or source exception.

The old algorithm preserved every free old interior vertex and cut each selected
old triangle separately. A vertex without an immutable physical owner caused a
thin new subdivision to invert under actual absolute Float32 conversion. Regional
encoding avoids that particular conversion failure, but retaining unnecessary
free interior constraints is avoidable work in the first place.

`terrain_layout_mode=free_region` subtracts the fixed road/pier geometry from the
whole selected region once, then triangulates its free parts. It keeps all actual
outer-boundary vertices and old vertices belonging to fixed triangles. Other old
free interior points/edges are replaceable DEM priors, not physical survey pins.
NumPy exact-coordinate membership avoids a Python tuple set for this selection.
The legacy mode remains the default for old recipes and keeps their array schema.
The saved reader binds the declared mode explicitly. No lib/native API changes.

The whole real planar regression is versioned as
`test/outshine/data/transport/free-terrain-planar-case.npz` with a provenance JSON.
The test first reproduces the legacy Q inversion, then requires positive D/Q
orientations, valid coverage, retained hard vertices and unchanged original input.
No topology, profile, contact, clearance, source-grid or solver limit is relaxed.

| Whole six-source case | Legacy | Free region |
|---|---:|---:|
| Old seed vertices passed to tessellation | 305 | 204 |
| Free polygon parts | 571 | 7 |
| Local vertices | 1,002 | 724 |
| Local facets | 1,896 | 1,340 |
| Actual absolute-Q topology | Refused | Pass |

All 106 actual outer vertices and new road geometry remain fixed. The new field
has 724 finite pins; eliminating interior degrees of freedom changes the terrain
approximation. Fewer vertices alone are not evidence of equal terrain quality.

## Independent results and images

Normal first attempt: generator 10.55398 s, saved reader 9.23203 s. These shared-host,
nice-10 diagnostic process measurements include their declared input/proof work;
they are neither frame times nor an A18/RTX budget. The 100-node MILP returns a
feasible candidate at its solution limit (status 4), then the full audit passes;
termination alone is insufficient and no optimality claim follows.

Independent complete-field reconstruction: all 33,470 XYZ facets and their
attributes match exactly; module/local-field arrays are byte exact. The complete
whole comparison takes 41.56784 s. All 18 prior assets remain retained by their
unchanged chain; all 6,904 previous spatial-volume records are exactly present
among 7,669 freshly constructed current records. All 124 pier contacts pass,
3,643 pair checks, maximum old Float32 contact error 15.170145 µm.

Actual current Buildings creates/verifies 65 whole bodies, and Vegetation replays
four complete plants against current terrain/transport/building reservations.
Fresh source and installed readers for module, volumes, Buildings and plants all
terminate 0; 121 actual reference imports per mode match their declared roots.
Installed generation reproduces all seven module artifacts exactly, with 55
matching library imports. Existing receipts were never overwritten.

296 source +296 installed controls pass, including the two new terrain cases and
nine regional controls. The latter retain their separate representation scope.
Same 212-file wheel as PR #711, SHA256
`41c7e9d2863b5677a92888754a239b200533799468bca1c7d251787a5ebd55e9`;
no reference-library changes. Producers use NumPy 2.2.4; actual consumer/control
environment uses NumPy 2.5.4, Python 3.13.5. Other versions match the preceding
report. Ruff, format, functions at most 120 lines and diff checks pass.

Four fresh whole 1280×720 perspective images were actually opened: before/after
overview and opposite network view, identical cameras/materials/profiles.
Both rings and the approach become visible as a connected addition. The sparse
coverage, weak transport contrast and coarse terrain remain clear limitations;
these are software-GL Blender diagnostics, not native engine/GPU acceptance.
Images remain private under `/tmp/outshine-transport-free-terrain-29ff36db2/comparison90/`.

## Fair terrain costs and approximation change

The failed legacy six-source case is not a valid speed baseline. Instead,
`stair_free_terrain_cost.py` compares the same accepted whole three-source body
from the same original 84-source baseline. Preload and body audit are outside
measurement; only terrain preparation and fit are timed/traced. Complete physical
and all-pier checks follow for both results. Both use the same 769 original
selected vertices/centroids as DEM controls and exactly the same selected region.

Frozen cost driver `aa5827132000786e47c20192f1f5178a4a6f72b7`, three unprofiled
normal-priority runs per mode and one separate Memray trace. Shared WSL2 host as
the preceding report; verification processes use nice 10. This is not an isolated
hardware or device comparison.

| Same three-source body, terrain stage only | Legacy | Free region |
|---|---:|---:|
| Vertices / facets | 790 / 1,473 | 528 / 949 |
| Median, s | 0.39124 | 0.26505 |
| Memray allocation events | 2,479,425 | 1,518,153 |
| Cumulative requested bytes | 339,174,001 | 204,610,225 |
| Tracked incremental peak, bytes | 2,448,372 | 1,867,944 |
| DEM mean absolute error, m | 0.189484 | 0.242222 |
| DEM RMS error, m | 0.228610 | 0.273751 |
| DEM maximum absolute error, m | 0.494746 | 0.734862 |

Maximum difference between the two actual fields is 0.696619 m, mean absolute
0.060314 m at identical controls. Both satisfy the original construction and
contact gates, but free terrain preserves less relief. These are measured costs
of different approximations, **not equal-geometry savings**. No default migration
of old products or universal winner follows. Cumulative allocation is not copied
traffic; explicit array byte counters are in each report. Internal GEOS/solver
copy traffic, the full joint MILP allocation/peak and device costs remain unknown.
The expensive height solver remains research, not a runtime recommendation.

## Reproduction and remaining work

Use the existing retained baseline/input paths from the delivered comparison.
`stair_growth_experiment.py --family seed --seed-root-form closed
--seed-crossings walk` selects the group and writes `recipe-0.json`; its first
`outshine-module-0` has a fresh saved-reader receipt. `stair_local_reference.py`
checks it against the complete 84-source reference; `stair_local_display.py`
exports only after whole-body equality. Existing occupancy/Buildings/plant
producer/readers consume that module through their existing interfaces.

For the fair terrain comparison, `stair_free_terrain_cost.py` accepts `--resident`,
repeated `--prior-module`, the original `--fixture`, its verified `--products`,
a fresh explicit `/tmp` `--output`, `--repeats 3` and optional `--profile` (Memray
1.20.0). It checks report/body/recipe bindings before any measurement and refuses
incomplete control coverage. Ordinary controls do not regenerate references.
Current original trials, traces, reader receipts, commands/terminal statuses and
images are retained only under `/tmp/outshine-transport-free-terrain-29ff36db2/`.

No SDK/foreign code was copied. SIGGRAPH-first graph/construction basis and primary
solver references remain in the linked joint-seed report. Regional owner encoding
and camera-relative actual GPU composition remain #717/#568/master, independent
of this case's unchanged absolute-Q success. Before delivery: settle the terrain
approximation trade-off, account for the retained height-fit costs and split the
functional change from broader unadmitted regional-render research. Native
integration/device gates and the complete worldwide goal remain open.
