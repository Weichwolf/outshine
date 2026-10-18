# Skip unused planning before broad source trials

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following
[protected terraces](terraced_terraces.md). This removes measured avoidable work
without changing the eight previously verified products, then examines the
existing complete source window. It does not admit new sources into the runtime.

## Same product, less preparation

`terraced_terrace_plan.py` now identifies nonempty source setbacks before offset/
contact resolution. Unchanged intermediate storeys no longer repeat those
operations. `terraced_terrace_frame.py` transforms only the window axes on actual
portal storeys, before constructing their world cutter polygons. The complete
original facade/window plan remains in the resulting blueprint; unrelated windows
are still present. No geometry, contact tolerance, IDs, route rule or capacity
limit changes. Existing source height, holes and metadata remain unchanged.

`terraced_terrace_plan_compare.py` loads the trusted own plan/frame sources from
`16d1bcf2c`, reconstructs all eight actual source/programme pairs and compares every
logical field. It then checks the direct mesh/face owners, shared component
sources/arrays, physical shell/stair/arrival arrays and provenance, volumes/areas,
frame arrays, and final direct/near presentation arrays. All compare exactly.
The prior actual ten-image comparison from #673 remains valid with its original
render commits/hashes; no new3D rendering is claimed. The upper-wing whole view
was optically opened again. The normal geometry displacement bound is unchanged;
this replay does not establish radiance/shadow/cache/native admission.

Frozen `41580c6fa`, Linux/x86-64/main `.venv`:11 source and11 installed-reference
checks pass, including all9 prior terrace controls and2 new factory/work controls.
Both actual-import checks locate94 loaded reference modules in the intended trees
and match their bytes to the frozen sources. The previously used installed wheel
is reused; dependencies come from the main `.venv`. The unchanged broader28
predecessor controls retain their #673 evidence; no fresh39-test run is claimed.

## Scoped work, allocation and timing

Each variant has three unprofiled plan builds per source/programme, excluding
imports, source acquisition and mass construction. Profiling and work counters
are separate replays. Memray1.20.0 includes Python/native allocation requests,
without native stacks;16 original traces/stats are retained in/tmp. Comparison
shares the host with verification/census work. These local timings describe a
trend, not a device budget or a universal speedup: source0 wings is slightly slower.

| Source / programme | Profile resolutions before → after | World window cuts before → after | Median plan ms before → after | Allocation requests before → after | Tracked plan heap peak MiB before → after |
|---|---:|---:|---:|---:|---:|
|0 cascade|8 →4|252 →98|219.31 →191.05|733618 →667753|1.304 →1.226|
|0 wings|8 →4|305 →110|233.93 →236.83|509982 →431552|0.514 →0.390|
|1 cascade|12 →4|264 →74|225.29 →194.42|664200 →579304|1.223 →1.132|
|1 wings|12 →4|329 →90|317.90 →217.49|738723 →636234|1.186 →1.072|
|2 cascade|14 →4|268 →63|265.64 →114.50|405296 →314452|0.455 →0.317|
|2 wings|14 →4|297 →69|229.64 →197.47|556144 →456784|0.963 →0.854|
|3 cascade|14 →4|508 →126|353.37 →279.48|784909 →623767|0.842 →0.512|
|3 wings|14 →4|467 →101|399.82 →281.36|1058067 →902840|1.395 →1.220|

Across these eight cases, profile resolutions fall96→32; window transformations
2690→731. The coordinate bytes presented to that transform fall215200→58480;
these are scoped inputs, not total copies. Allocation requests fall5450939→4612686
(15.38%); cumulative allocated traffic580.964→496.624MiB. Traffic is not resident
memory. Traced peaks concern the planning replay with the source mass already
present; do not substitute them for the complete building, source decoder,
background workers or device memory. Physical/output arrays are unchanged.

## Whole original source window

`terraced_mass_census.py` uses the same pinned988-source window and the existing
80-request geometry/context/seed cohort. `data/terraced-city-baseline.json`
contains only fixed test memberships, original source/request digests and receipts
from the earlier #585 full source scene. This is comparison input, not a runtime
place recipe or a new task board. It protects the12 prior functional owners,
including all8 podium assets. Special structures retain their earlier classifier.
The old cohort retains supplied roof tags and its original semantic eligibility;
it is not a new claim of worldwide classification or recipe coverage.

The generator tries the two existing programmes in stable source-derived order,
stopping at the first direct success. Detailed physical-floor construction is
forbidden throughout the census. Every source ID is accounted, all receipts and
cohort digests match, and original footprint/property hashes are compared again
after all candidate construction. Failed/unselected/protected sources retain
ownership; their previous geometry is not rebuilt by this census.

Final frozen census `f747bb5c0`:

| Status | Original sources |
|---|---:|
|outside unchanged request cohort, prior route retained|904|
|existing functional owner protected|12|
|direct candidate produced|71|
|both mass programmes rejected, prior route retained|1|
|total|988|

The80-request cohort includes8 of the protected owners; the remaining72 requests
produce71 direct candidates (40 cascade/31 upper-wing programmes). They contain
14592 window records and157 planned portals. Their direct draw triangle counts
sum347825; this is not an emitted whole-city mesh or a claim of71 new closed,
walkable assets. The largest individual direct owner's NumPy payload is3538466
bytes; that excludes merged-draw scratch/Python metadata/source decode. The final
census takes23.969s after input acquisition; whole-worker peak RSS411.391MiB includes
imports/source decode and is not a complete sandbox budget. Both values are local
observations. No allocator/copy/device bound is derived for the complete census.

Eleven failed programme attempts are recorded: nine empty/disconnected/undersized
profiles, one disconnected actor-clear interior and one missing shared stair
reservation. Nine sources succeed with the second programme. The one final rejected
source cannot obtain a contained core with cascade or a valid upper profile with
wings. Its original source remains; no threshold was relaxed to raise coverage.

This is a broader2D/direct-surface trial. New-source closed-volume/physical portal/
stair/arrival proofs, normal near/far selection, whole-city rendering and native
costs remain open. The12 previous functional owners and only the eight earlier
actual control programmes retain their existing physical proofs. The census uses
the unchanged provider window; it does not upgrade any Transport handoff evidence.

## Whole-window2D inspection

`terraced_mass_map.py` at `1d2db1793` produced a2880×1620 PNG which was optically
opened at original resolution. Both panels retain the complete988-source extent
and open courtyards. The right panel overlays213 actual unique storey-profile
polygons from the71 candidates; green marks only the location/outline of the12
protected owners, not a reconstruction of their existing meshes. Receipt/source
hashes and counts match the final census. The map exposes the distribution and
shape variation without cropping the window. It is a2D research map, not a3D city
image or evidence that all new candidates are physically traversable. Its PNG,
input report and producer receipt remain in/tmp.

## Next step and reproduction

Before replacing any whole-window source output, construct/admit its physical
counterpart with the existing independent geometry and continuous access gates.
Retain local failures and the earlier functional/special owners. Then evaluate
actual full-window near/direct demand and images; complete near arrays remain
expensive. Selective near components and richer roof/facade programmes stay open.
This local cost/candidate block does not close #618, #32/#33 or the overall goal.

The existing scope/component composition references in [the terrace report](terraced_terraces.md)
remain applicable; no new modeling operator, SDK oracle or Ghidra run is claimed.
Use `.venv`, `TMPDIR=/tmp`, `PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache`,
`XDG_CACHE_HOME=/tmp/outshine-buildings-cache`,
`MPLCONFIGDIR=/tmp/outshine-buildings-mpl` and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`.

```sh
git show 16d1bcf2c:test/experiments/buildings/terraced_terrace_plan.py > /tmp/outshine-buildings-plan-before.py
git show 16d1bcf2c:test/experiments/buildings/terraced_terrace_frame.py > /tmp/outshine-buildings-frame-before.py
.venv/bin/python -m unittest -v verify_terraced_terraces
.venv/bin/python test/experiments/buildings/terraced_terrace_plan_compare.py --reference-plan /tmp/outshine-buildings-plan-before.py --reference-frame /tmp/outshine-buildings-frame-before.py --traces /tmp/outshine-buildings-demand/traces --output /tmp/outshine-buildings-demand/compare.json
.venv/bin/python -m memray stats --json -o /tmp/outshine-buildings-demand/0-cascade-after-stats.json /tmp/outshine-buildings-demand/traces/0-cascade-after.bin
.venv/bin/python test/experiments/buildings/terraced_mass_census.py --output /tmp/outshine-buildings-demand/census.json
.venv/bin/python test/experiments/buildings/terraced_mass_map.py --census /tmp/outshine-buildings-demand/census.json --output /tmp/outshine-buildings-demand/map
```

All research outputs/inputs/traces/logs stay in/tmp. The ordinary whole-city producer
and shared/native libraries are unchanged; there is no new specialist gallery.
