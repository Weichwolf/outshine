# Physical admission of the complete proposed terraced cohort

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), after the
[complete source census](terraced_source_demand.md). A direct surface and a
reserved core do not establish a usable physical building. This study constructs
the proposed near assets and independently checks them before replacing any
existing whole-window source route. It does not admit assets to the native engine.

## Real source failures and bounded corrections

The first narrower physical pilot attempted all71 proposed programmes.65 passed
that pilot; six failed. `data/terraced-admission-controls.json` retains those six
complete provider rows, original footprint WKB, properties/context and the six
pinned OpenFreeMap receipt hashes. Coordinate containers alone are converted to
JSON arrays. Neither architecture selection nor modelling depends on place names.

Three source cases reject their raw upper prism triangulation as nonmanifold.
`terraced_mass_geometry.physical` first tries the unchanged raw construction.
Only that specific source rejection permits a1e-10m precision-grid retry; other
errors still fail. The grid is removed from GEOS metadata before comparing the
new polygon with the original, so the comparison does not silently snap both
operands to the same grid. The sum of symmetric-difference area times band height
must remain within the existing1e-7m³ volume tolerance. Original logical WKB,
source IDs and supplied heights remain unchanged. This is an explicit bounded
contact approximation, not a claim of preserving every microscopic topological
feature. The final shell still undergoes independent section/volume checks.

One source already has a valid closed mesh with explicit vertex roots, but its
arrival checker discarded that topology and attempted coordinate-only welding.
`terraced_facade_access` and `entrance_clearance` now retain those existing roots;
old mesh dictionaries without roots keep their previous path.

Two terrace routes exposed the need to plan against the actual room contour.
Recomputing an inward polygon offset after rotating it into world coordinates
can differ at almost collinear corners. In the retained431 counterexample the
world-recomputed interior differs from the actual canonical cavity by0.00589m²;
the original actor route intersects the actual wall. `world_interior` now places
the same canonical offset/intersection used by the physical floor. The route
planner also reserves an additional1mm from obstacles; physical actor radii,
height, support and collision tolerances remain unchanged. Only actual setback
storeys compute this interior, once per storey before individual terraces.

Arrival validation still rejects inclined boundaries near the finite actor tube.
It no longer rejects a whole ground entrance because a distant upper-storey
triangulation sliver is inclined. The filter uses actual triangle height and XY
convex-hull intersection with the complete conservative tube; it does not discard
small triangle areas. The near-inclined negative control still fails.

## Independent scope

`terraced_mass_admission.inspect` checks:

- Root-aware closed shell/stair/landing roundtrips, saved area/volume and a separate
  signed triangle-volume calculation, at the existing1e-7 tolerances.
- Actual triangle-edge shell sections against the logical union of source bands
  minus open-floor/stair/entry/window/portal prisms, at every logical height slab.
  The independent2D sweep must reproduce shell volume within1e-7m³ and each section
  within1e-7m². The checker transforms the actual saved vertices back to the
  declared core frame; it does not rebuild a second physical shell as its oracle.
- Every closed frame/pane/accent component, and independent prism volume/area and
  supplied upper height for all added decks, guards and copings.
- The actual stair tread poses with0.30m torso/0.10m foot samples and1.90m headroom.
  Possible facade/terrace components are conservatively selected before adding
  their native interval queries; the old three structural parts are all retained.
- Continuous ground arrival including lower legs, the complete declared approach
  to the stair front, actual guarded terrace sections and every declared
  core-to-portal-to-deck route with the existing conservative tubes.

The stair test samples16 angular points plus the centre on each disk. It is not
a continuous stair/character-controller proof. Ground and terrace tubes do not
establish unrestricted room/terrace roaming, highest-roof access, collision with
neighbors or external terrain, native caching, radiance/shadow/time behavior,
night lighting, frame costs or device admission. Basic rooms remain sufficient
for the current exterior-first task.

`terraced_admission_probe.py --census` requires all988 original source memberships
and footprint/property identities, checks the protected12 prior functional owners
and attempts exactly the71 originally proposed programmes. It retains the full
old census alongside the outcomes. A failure records its source/reason and keeps
the earlier source route; no failed new near asset becomes a replacement merely
because its direct surface exists. Without `--census` it audits the six fixed
controls and optionally exports their actual near meshes for local rendering.

## Reproduction

Use the project's `.venv`, `TMPDIR=/tmp`, and temporary Python/renderer caches:

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
.venv/bin/python -m unittest -v verify_terraced_mass_admission
.venv/bin/python test/experiments/buildings/terraced_admission_probe.py --output /tmp/outshine-buildings-admission/controls.json --models /tmp/outshine-buildings-admission/models
.venv/bin/python test/experiments/buildings/terraced_mass_census.py --output /tmp/outshine-buildings-admission/direct.json
.venv/bin/python test/experiments/buildings/terraced_admission_probe.py --census /tmp/outshine-buildings-admission/direct.json --output /tmp/outshine-buildings-admission/cohort.json
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a .venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-admission/models/control-0-geometry.json --output /tmp/outshine-buildings-admission/images --samples 16
```

The census and render require a clean frozen experiment. Inputs come from the
existing provider source cache; this does not request an automatic source refresh.
On the current headless host Eevee uses llvmpipe through Xvfb. All private outputs,
logs, virtual-display files and renders remain under `/tmp`, never `/var/tmp`.
No specialist images/server/gallery are published.

The local SIGGRAPH2006 scope/component modelling and pinned SDK/Ghidra offset
references remain the primitive background documented in
[terraced_terraces.md](terraced_terraces.md). Their previously optically checked
figures and binary receipts are reused; this correction does not claim a new Esri
architectural recipe, interior implementation or fresh oracle comparison.
