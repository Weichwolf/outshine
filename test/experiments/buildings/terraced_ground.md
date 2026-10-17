# Ground approaches with the checked finite actor radius

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following the
[physical cohort](terraced_admission.md) and [whole-city study](terraced_city.md).
The earlier physical audit rejected two courtyard buildings because their
declared ground approaches intersect the actual wall. This correction changes
route planning while retaining the existing physical checking boundaries.

## Failure and correction

The complete provider counterexamples are retained in
`data/terraced-ground-controls.json`: sources
`Paris/(14, 8298, 5635)/5409740/16/430` and
`Paris/(14, 8298, 5635)/3953080/19/115`, both paired wings. Original footprint WKB,
properties, context, IDs, six pinned OpenFreeMap receipt hashes and the original
failed approach WKB remain available. JSON arrays replace coordinate containers.

The old ground planner erodes the wall region by the nominal0.30m actor radius,
but already expands stair obstacles by the conservative polygonal cover used by
the checker. The same cover must apply to the walls:

```text
r = 0.30 / cos(pi / (4 * 32)) = 0.3000903816123906 m
wall-clear centre region = source.buffer(-(wall_thickness + r), mitre)
blocked stair region = (core - front_landing).buffer(r, quad_segs=32)
```

At1.06m above the original ground, the old complete torso tubes intersect the
actual closed shell by0.000111446830m² and0.000152304658m² respectively. Their
original continuous routes still fail on the corrected assets. The new routes
have at most1.92e-12m² intersection in the displayed actual section, below the
unchanged1e-8m² boundary, and pass the unchanged
continuous foot-support/lower-leg/torso tests. The route anchors remain exact.
The1920×1080 paired2D image is optically checked; the submillimetre displacement
is smaller than a display pixel, so the measured overlap accompanies the figure.

An extra1mm reserve was evaluated and rejected: on the already valid source
`Paris/(14, 8298, 5635)/5335120/23/657`, it reverses the stair orientation.
That complete source and its original stair poses/opening/flip are a separate
preservation control in the same fixture. The final planner uses the actual
conservative radius without the unnecessary extra reserve. Terrace planning and
its previously scoped reserve are unchanged.

## Retained plans and physical scope

Frozen final implementation: `71aeda194497661881d883a0f00cb0ccece3823f`.
The previous core from `2efa55c558acbdf080eccacc86362d7a0ffa39fb` is replayed on
all71 original proposed programmes. Every mass-plan field except the approach
WKB/method compares exactly: original source/storey/terrace profiles, heights,
entry, core frame/opening/front/stair poses, architecture choices and candidate
counts.23 route WKBs change, no routing method changes; the largest route-length
change is0.000366849m.12 protected previous functional owners are outside this
new mass cohort and retain their earlier pipeline.

The existing bounded dogleg/40×40 graph and its work limits remain unchanged.
The paired71-plan run takes6.207s before and6.537s after under simultaneous
nice10 checks; it is one interleaved diagnostic sample, not a performance or
device acceptance result. Both branches perform the same two geometric buffers;
this fix makes no allocation, cache, GPU or native frame-cost claim.

The complete frozen physical audit now passes all71 proposed programmes, versus
69 in the earlier reference, bound to the same988-source census and protected12
owners. It checks2796 independent shell slabs,720 closed terrace components,
92451 closed facade components,9882 stair poses and151 declared terrace portals.
Maximum independent section XOR is3.3621e-9m²; maximum shell volume discrepancy
is1.3967e-8m³, below the unchanged1e-7 boundaries. The worker completes in1105.956s
after source loading; its peak RSS is423288KiB. This includes independent offline
checking and concurrent nice10 work, and supplies no runtime/device budget.

All66 affected tests pass from source and again against the existing installed
reference target on Linux x86_64/Python3.13.5. The runners complete in574.084s and
578.683s; both verify the actual paths/bytes of107 loaded reference modules
against the frozen source. No new isolated dependency install is claimed.
The two failed old paths remain failing negative controls. The valid657 stair
retains its exact opening, orientation and poses. Ruff/format and doc checks pass.

The checker scope remains the one documented in the physical admission report:
closed parts, independent shell sections/volume, sampled stairs, continuous
ground arrival/approach and declared terrace routes. Continuous stairs,
unrestricted room/terrace roaming, highest-roof access, native controllers,
asset persistence, radiance/shadow/night and device budgets remain open.

## Actual images and remaining exterior quality

The two actual1920×1080 Eevee images from producer `1f41b7581` are reused with
their original render provenance. The final actual near draw arrays, including
their dtypes, compare exactly with both original NPZ inputs. Their rendering,
material and projection source receipts remain unchanged. Both images and the
final paired2D figure are optically opened. This is a functional route repair;
the existing bare highest roofs and pale repetitive facade/window treatment
remain visible deficits for the following exterior work.

## Reproduction and retained evidence

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
.venv/bin/python -m unittest -v verify_terraced_ground
.venv/bin/python test/experiments/buildings/terraced_admission_probe.py --controls test/experiments/buildings/data/terraced-ground-controls.json --output /tmp/outshine-buildings-ground/controls.json --models /tmp/outshine-buildings-ground/models
.venv/bin/python test/experiments/buildings/terraced_admission_probe.py --census /tmp/outshine-buildings-terrace-census-f747bb5c0.json --output /tmp/outshine-buildings-ground/cohort.json
```

Use the pinned full census from the source-demand study, or reproduce it with
that study's producer first. The audit requires the unchanged complete988-source
membership, source hashes, protected owners and original71 programmes. It refuses
dirty modelling inputs. `--controls` and `--census` are mutually exclusive;
the old six-counterexample set remains the default.

Retained local evidence:

- `/tmp/outshine-buildings-ground-audit-71aeda194/cohort.json` and
  `/tmp/outshine-buildings-ground-audit-71aeda194.log`: complete physical outcomes,
  unchanged original census and source/implementation receipts; terminal0.
- `/tmp/outshine-buildings-ground-tests-71aeda194-{source,installed}.json`,
  with logs `/tmp/outshine-buildings-ground-{source,installed}-71aeda194.log`:
  66+66 tests and actual matching107-module imports; both terminal0.
- `/tmp/outshine-buildings-ground-replay-71aeda194.json`: exact non-approach
  mass-field replay against the original core for all71 programmes.
- `/tmp/outshine-buildings-ground-controls-71aeda194/ground-routes-before-after.{png,json}`:
  actual same-body sections and continuous old/new route outcomes.
- `/tmp/outshine-buildings-ground-image-replay-71aeda194.json`: complete actual
  draw-array replay and original image/input hashes. Original two model NPZ/JSON,
  actual images and render reports remain under
  `/tmp/outshine-buildings-ground-controls-1f41b7581/{models,images}/`.

The extra-reserve audit/suite attempts were stopped as superseded after the657
preservation failure; they are not counted as passing final checks. The final
radius-only full audit and both suites complete afresh at the stated revision.

All new private outputs, logs, virtual-display files and renders use `/tmp`.
No specialist gallery, server, shared/native/library API or provider refresh is
introduced. The previously optically checked SIGGRAPH2006 scope/component and
pinned SDK/Ghidra offset references remain documented in
[terraced_terraces.md](terraced_terraces.md); this work corrects the existing
planner's radius rather than adding a modelling operator.
