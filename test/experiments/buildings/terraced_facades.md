# Physical facades on the terraced mass prototype

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following
[the mass/open-floor study](terraced_mass.md). This experiment adds a visible
exterior layer to the same source-stable masses; it is not a complete architecture
family or a whole-city integration.

## Geometry and ownership

`terraced_facade_plan.py` places metrical axes on the actual storey-profile edges,
including courtyard edges. A source-centroid phase keeps window stations aligned
between floors and setbacks. The cascade programme uses2.25m-wide windows and
horizontal bands; paired wings use1.45m windows and vertical piers. Corners,
insufficient interior margins, the existing entrance and reserved core are excluded.
Plans retain stable source/storey/window identifiers and an explicit rejection list;
the bound is768 windows before opening meshes are expanded. Neither camera nor
place name chooses architecture.

`terraced_facade_geometry.py` composes the previous simple floors and cuts actual
through windows. All CSG stays in the canonical core scope. Window and cutter
metadata, the three structural components and detail positions are placed once.
`terraced_facade_details.py` reuses the existing opening-ring templates and adds
closed8mm panes, mullions, sills and accent members. The detail mesh owns explicit
part/material arrays. Flat metric normals/UVs are generated for presentation;
no photographic assets or third-party code are included.

The local SIGGRAPH2006 paper p.3/Fig.4 was optically inspected again: scope,
component, split and repeat composition are the reference; these two programmes
are our own procedural choices. Existing scope/offset SDK/Ghidra evidence from
the preceding mass study is reused without claiming a fresh SDK oracle comparison.

## Physical controls

Frozen `a166be96a`, Linux/x86-64, `.venv`:21 source/21 installed-wheel controls
pass, including the12 previous mass/open-floor controls. Four unchanged actual
OpenFreeMap fixtures, each with both programmes, produce2690 real window slots.
Every shell remains closed, retains source height and passes1e-7m³ volume and
1e-7m² surface roundtrips. Removed volume independently equals the sum of wall
thickness × window width × window height. Three horizontal rays through every
window verify actual through clearance. All actual frame/pane/accent parts are
decoded as closed positive-volume solids. The previous stair and pad arrays are
exactly retained;872 stair poses retain their existing support/headroom gate.

The complete ground arrival also includes every facade part whose conservative
3D bounds can affect it. The unchanged continuous prismatic foot-support/torso
gate is extended by a continuous lower-leg/foot tube over critical height slabs.
A source-ground band crossing the threshold failed that added gate; the band
and ground piers are now interrupted by the existing entrance keepout. A deliberate
4cm raised threshold fails, while a flush actual threshold is included and passes.
No old gate/tolerance was relaxed. Upper stair transitions remain sampled;
window traversal, glass breakage, terrain and a native controller are not proven.

The reused installed wheel matches all205 library modules at that revision; the
separate actual-import check locates81 loaded reference modules in its installed
target. Main `.venv` supplies dependencies. This is not an isolated-environment,
whole-library or native acceptance claim.

## Actual picture comparison

Ten current1920×1080 Eevee images were optically opened: four whole-building
before/after pairs on actual sources0/3 and one window close-up for each programme.
Source, camera, daylight and material definitions match within each pair. Final
input arrays/camera/materials exactly match six earlier valid images, which are
reused; the two corrected cascade views and two close-ups are fresh renders.
Research PNGs, original inputs and replay/render receipts stay exclusively/tmp.

Windows, frame depth, sills, bands and piers are now legible on the stepped forms.
The repetition is still strong. Roofs/terraces remain bare and lack portals/rails;
the present Eevee glass looks uniform and does not demonstrate through-glass
interior/night lighting. These pictures do not show a complete-city or native gain.

## Incremental near cost, not an accepted runtime budget

`terraced_facade_cost.py` measures fresh processes at the same revision. This table
adds higher facade quality to the unchanged baseline; it is not an equivalent-
quality speed comparison. Source/import/mass planning is excluded from construction
time, while facade planning is included. Runs share the host with verification work.

| Source / programme | Baseline triangles | Near triangles | Windows | Returned arrays MiB | Build seconds |
|---|---:|---:|---:|---:|---:|
|0 cascade|1446|41282|252|5.243|0.835|
|0 paired wings|1330|48610|305|6.209|0.821|
|3 cascade|2220|81768|508|10.280|1.431|
|3 paired wings|2438|74394|467|9.500|1.481|

Each after case retains26 distinct NumPy array buffers; closed detail parts range
1593–3187. A separate traced replay peaks at12.088–23.770MiB; native/GPU allocations
are not included. Fresh-process peak RSS is77.664–100.699MiB including imports.
These values are not added. Cumulative allocation/copy counts, complete memory
traffic, devices, cache, rotation/motion and full-city residence remain unmeasured.

The full near arrays are too expensive for general city residence. The next #618
cost step is a logical facade plan before CSG/detail expansion, direct far surfaces
and selected reusable near components. Shadow/reflection/interaction demand and
unchanged logical access must remain separate. No raised budgets or reduced world
content follow from these current costs.

Reproduce in `.venv` with `TMPDIR=/tmp` and the experiment/library `PYTHONPATH`:

```sh
.venv/bin/python -m unittest -v verify_terraced_mass verify_terraced_mass_rooms verify_terraced_facades verify_terraced_facade_access
.venv/bin/python test/experiments/buildings/terraced_facade_probe.py --output /tmp/outshine-buildings-facades
.venv/bin/python test/experiments/buildings/terraced_facade_cost.py --source 0 --programme cascade --mode after --output /tmp/outshine-buildings-facade-cost.json
```

Terrace/roof programmes, neighboring occupancy, direct far/selected near products,
complete988-source selection, cache persistence and library promotion remain in
#618/#33. Existing complete functional products keep their original path and gates.

The next measured cost step is now documented in
[direct facade products](terraced_facade_products.md): plan before physical
expansion, equivalent-coverage batch comparison and retained lazy near ownership.
