# Whole automatic transport seeds with local Float32 height repair

Research #713/#714/#725/#729; broader regional representation #717 remains separate.
Generator commit `ffc8818d469e9145f945acc383eb2c2accad6e84` produces and independently
verifies the whole six-source, four-walking-contact candidate from the unchanged
84-source baseline: 90 whole original line parts, 669.7534536827 m in the new
module. The earlier delivered 87-source product and receipts remain unchanged.
This is a bounded Python delivery, not native/world admission. The independent
regional/renderer study remains Draft PR #720 and is excluded from this delivery.

## Inputs and construction

Original MVT, DEM, source IDs, lengths and baseline hashes are documented in the
[historical joint-seed trials](https://github.com/Weichwolf/outshine/blob/e86f905ca/test/experiments/transport/stair_seed_dependencies_comparison.md) and
[delivered 87-source comparison](stair_joint_crossing_comparison.md).
Same named Wien ENU frame and original 84-source baseline; both rings, the entire
254.6968814-m approach, short road/cycleway and connecting footway are retained.
Original nodes remain distinct from four explicitly procedural walking contacts.
No motor/cycle turns onto footways, clipped substitute group or source exception.
The selector derives whole dependencies before freezing heights, with at most
eight sources/four geometric walking contacts per recipe and three geometry
attempts. Original shared nodes and procedural crossings remain distinct.
Private/conditional access, wrong levels, ambiguous multiple crossings and
unresolved interiors are explicitly refused. Index/scan agreement is checked
on the actual whole source input; old selection remains the default unless
`--seed-crossings walk` is requested.
Current functional-topic selection agrees on all roots/recipes/reasons/dependencies,
recipe SHA256 `798343547377a1bb8bcf45f5b16f1ec4e722f27f5dab69a727e309c9c1d6d258`.

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

Normal first attempt: generator 9.55237 s, saved reader 9.26304 s. These shared-host,
nice-10 diagnostic process measurements include their declared input/proof work;
they are neither frame times nor an A18/RTX budget. The whole body now uses the
joint cone fit plus bounded local Float32 repair, without a hidden MILP fallback.

Independent complete-field reconstruction: all 33,470 XYZ facets and their
attributes match exactly; module/local-field arrays are byte exact. The complete
whole comparison takes 65.13860 s under concurrent nice-10 control tests.
All 18 prior assets remain retained by their
unchanged chain; all 6,904 previous spatial-volume records are exactly present
among 7,669 freshly constructed current records. All 124 pier contacts pass,
3,643 pair checks, maximum old Float32 contact error 15.170145 µm.

Actual current Buildings creates/verifies 65 whole bodies, and Vegetation replays
four complete plants against current terrain/transport/building reservations.
Fresh source and installed readers for module, volumes, Buildings and plants all
terminate 0; 121 actual reference imports per mode match their declared roots.
Installed generation reproduces all seven module artifacts exactly, with 55
matching library imports. Existing receipts were never overwritten.

300 source controls passed on the research branch; the functional topic excludes
the nine regional controls and passes 291 source +291 installed controls on `9d0e48f0a`,
122 matching library imports per mode. Source/installed actual module and consumer
readers also pass there; installed generation reproduces all seven artifacts.
All 29 functional source/data files match the original frozen generator source
byte for byte. Only README/report changes follow those checks.
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
Images remain private under `/tmp/outshine-transport-rounded90-ffc8818d4/comparison90/`.

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
copy traffic and device costs remain unknown. Complete height-fit allocation and
peaks are measured separately below. MILP remains a research comparison, without
an automatic fallback to it.

## Complete height alternatives and bounded local repair

Identical actual 520 vertices/765 faces, four flat groups/237 level vertices,
original DEM targets and reserved anisotropic grade frames. The complete height
fit is timed/traced, including memberships, matrices, solvers and internal checks;
preload, source/body audit, imports, independent observation and export are excluded.
All three unprofiled attempts and the separate trace are retained, including
negative results; no favorable retry was selected. Frozen cost driver `019517afb`,
input SHA256 `b34c8358955a70521373fbe7d584a317e3bf7af9aa65cec0cb02a022a8949da2`.

| Complete height stage | Fixed procedural means | Joint cone | Whole MILP | Joint cone + local repair |
|---|---:|---:|---:|---:|
| Median of three normal-priority runs, s | 0.04310 | 0.11513 | 1.59135 | 0.07206 |
| Memray allocation events | 46,854 | 99,927 | 2,530,406 | 105,923 |
| Cumulative requested bytes | 7,714,021 | 16,307,603 | 2,579,936,063 | 17,693,884 |
| Tracked incremental peak, bytes | 3,358,199 | 3,374,031 | 21,667,189 | 3,373,856 |
| Actual absolute D/Q-valid result | None, infeasible | No, Q grade 2.2733 | Yes | Yes |

The two feasible alternatives meet the same unchanged construction bounds; their
shapes are not identical. Maximum original DEM height error is 0.17172665 m for
MILP and 0.17171629 m after local repair. The valid MILP stops at 100 nodes, not an
optimality proof. Its 13,513×521 sparse matrix has 39,266 nonzeros/473,280 array
bytes; the repeated solver work dominates cumulative allocation. The repaired
method uses two cone solves with 12+12 iterations, preserving the shared eight-
second/100-conic-iteration budget. Time differences between inexpensive methods
are noisy shared-host measurements; no claim that repair accelerates the raw cone.

`stair_joint_height_rounding.py` first binds exact Float32 flat-region levels and
rounds heights. It examines up to 100 finite combinations of the current height
and immediately adjacent Float32 codes for vertices of a violating face. A flat
group shares one variable, including members outside that face. Only affected
incident facets are evaluated per proposal; final whole D/Q physical audits still
run. Candidates must improve the actual violation score and remain within 5 mm
of the continuous result. No XY, width, crossfall or original source changes.
Time is checked between bounded batches; missing/expired/unsuitable candidates
return absence. There is no general convergence or global optimality claim.

In the complete real case, 27 proposals test five affected faces (135 face tests).
One rounded vertex changes; maximum displacement from the continuous fit is
8.132356 µm. Reserved normalized grade is 0.87410881 in both Double and Float32,
with exactly flat groups. Candidate heights/grades/errors occupy 1,512/1,080/1,512
array bytes; whole coefficients and aliases cost 36,720/4,160 bytes. These explicit
arrays are not a full copy-traffic or heap model. The real thin-face regression,
propagated flat-group neighbor, bounded absence and actual source-reader controls
are in `stair_joint_rounding_checks.py`.

## Terrain approximation decision

A separate full regeneration uses the **same byte-exact whole three-source road
body** and exactly reproduces the terrain-cost arrays. Independent whole-field
comparison and four actual whole 1280×720 before/after images pass and were opened.
Only the free terrain recipe changes. `test/scripts/pixels.py` finds 0.3790% changed
pixels in overview and 0.2077% in the opposite view; worst channel difference is
6/255, mean 0.003304/255 and 0.001317/255. Physical source surfaces, boundaries,
piers and contacts remain fixed; no visible source deletion or new seam appears.
The measured relief loss is a local approximation trade-off, not equal geometry.

Decision: retain `free_region` for these newly planned unbound seed regions, with
original DEM sampling, all construction/contact gates and explicit recipe binding.
Keep old products in legacy mode. This is a plausible bounded terrain recipe with
measured local error/image effect, not a universal DEM error tolerance or a reason
to flatten other regions. Downstream consumers use the actual resulting terrain.
Inputs/images/terminal status remain under
`/tmp/outshine-transport-terrain-quality-85844d4fc/`; the original 87-source product
is unchanged. The whole six-source functional candidate has its own current images.

## Reproduction and boundaries

Use retained original baseline/input paths from the delivered comparison.
`stair_growth_experiment.py --family seed --seed-root-form closed
--seed-crossings walk` selects the group and writes `recipe-0.json`; its first
`outshine-module-0` gets a fresh saved-reader receipt. `stair_local_reference.py`
checks it against the complete 84-source reference; `stair_local_display.py`
exports only after whole-body equality. Existing occupancy/Buildings/plant
producer/readers consume that module through their existing interfaces.

`stair_free_terrain_cost.py` and `stair_joint_height_cost.py` accept `--resident`,
repeated `--prior-module`, the original `--fixture`, its verified `--products`,
a fresh explicit `/tmp` `--output`, `--repeats 3` and optional `--profile` (Memray
1.20.0). They bind original reports/body/recipe before measuring. The former uses
the valid three-source87 body; the latter the complete six-source body/targets.
Ordinary controls do not regenerate references. Current original trials, traces,
reader receipts and commands/terminal statuses remain only under
`/tmp/outshine-transport-rounded90-ffc8818d4/`, terrain-cost evidence under
`/tmp/outshine-transport-free-terrain-29ff36db2/outshine-soil87-public/`, and full
height comparison under `/tmp/outshine-transport-local-rounding-019517afb/`.

SIGGRAPH first: Chen2008 page2/Figure2 (previously optically checked) supports
separating network and geometry; Cherchi2022 page2/Figures2–3 (also optically
checked) motivates explicit robust representation/classification. Neither proves
this own finite local search. Primary solver references: [Clarabel](https://clarabel.org/stable/python/getting_started_py/)
and [SciPy/HiGHS MILP](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.milp.html).
No SDK/foreign code is copied. General binary64/affine topology remains #568;
regional owner encoding and actual camera/GPU composition remain #717/master.
Native integration/device gates and the complete worldwide goal remain open.
