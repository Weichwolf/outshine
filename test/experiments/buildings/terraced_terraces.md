# Protected decks and real portals on setback terraces

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), extending
[terraced masses](terraced_mass.md), [physical facades](terraced_facades.md)
and [direct exterior products](terraced_facade_products.md). This is an actual
exterior/function increment on four existing source fixtures, not a complete
architecture family or whole-city/engine acceptance.

## Composition and function

`terraced_terrace_plan.py` preserves source/storey heights and profiles. At each
setback it prepares a16cm deck whose walking surface matches the upper storey
floor, a12cm solid parapet along exposed outer/courtyard drop edges, and a metal
coping at1.10m above the deck. These parts stay below the original building top.
They are small independently closed components, shared between direct and near
products; constructing them never requires the detailed physical building.

Portals use existing source-aligned window axes. One suitable window bay is
replaced by a1.20m ×2.15m open portal; unrelated windows retain their IDs. A route
connects the previously verified stair front to the door approach, followed by
a straight supported exterior passage. Both portal and circulation are planned
before floor/CSG/detail expansion. The bounded existing dogleg/grid route solver
works in core axes; world-axis-only doglegs had missed valid upper-wing routes.
Unserved components retain protected roof surfaces and explicit reasons; they
are not reported as accessible terraces.

The facade's optional local portal descriptors create actual through cuts and
three-sided frames. Ground/upper accent keepouts interrupt any band/pier crossing
the passage. Direct wall panels carry the same portal apertures; their frames
reuse the same small templates. There is no sill/raised threshold across a door.
The previous shell/floor/stair/arrival owners remain independent. Physical near
construction stays lazy and retained in the existing `FacadeProducts` owner.
No new persistent cache, global manager, material family or native API is added.

## Source contacts and independent checks

Original world profile WKB and source IDs are retained. Portal CSG uses the
canonical local core frame; world deck polygons avoid transforming source cap
contacts into invalid rings. Raw setback differences can nevertheless contain
very long, effectively zero-width contact whiskers. Their tiny area does not
justify extruding spurious vertical guard sheets. A source0 counter has a5.343m²
surface mismatch despite matching volume; its original mesh/input is retained
in/tmp. Comparing only volume or passing the raw polygon's validity was insufficient.

`terraced_terrace_frame.resolved` explicitly resolves these contacts on a1e-10m
coordinate grid and applies a mitred opening with1e-6m radius. Every invocation
rejects a source-area symmetric difference over1e-7m². This is a bounded minimum-
feature approximation, not an exact topology/Hausdorff preservation claim: long
sub-resolution whiskers may disappear. Original input WKB is not overwritten.
Guard placement still follows the original lower-profile drop edges; reusing a
normalized lower edge for that decision had failed the independent coverage gate.
No old physical tolerance was raised.

`terraced_terrace_contacts.py` compares alternatives on the same actual inputs.
The initial24 original deck polygons alone make a grid-only approach look adequate;
the complete deck/guard/coping assemblies expose its remaining failures:

| Method | Input programmes | Rejected programmes | Failing components / emitted components |
|---|---:|---:|---:|
|raw polygons|8|0|35 /72|
|grid only|8|0|5 /75|
|grid + mitred opening|8|0|0 /75|
|simplified native CrossSection|8|5|0 /30|

The rejected CrossSection programmes exceed the unchanged source-area bound;
its surviving components do not establish equivalent coverage. This is a small
quality/geometry comparison, not a speed or global optimality claim. Full records
at `a2c5b7da6` include failures, coordinates, triangles and elapsed work. The earlier
24-deck comparison and original counter are retained alongside the assembly run.

Frozen physical implementation `16d1bcf2c`:37 source and37 installed-reference
controls pass, including the previous28 and9 new checks. Both actual-import audits
locate94 reference modules in the intended source/installed tree and compare all
loaded bytes to the frozen source. The existing installed wheel is reused; the
main `.venv` supplies dependencies. No isolated environment is claimed.

New controls independently verify original terrace area, stable replaced-window
IDs, closed positive components with area/volume roundtrips within1e-7m²/m³,
source top height, unchanged stair/arrival arrays and actual portal removed volume
from wall thickness × aperture area. Every retained portal passes continuous
support/lower-leg/torso tubes from the stair front through the actual doorway.
Critical height events come only from actual boundaries that can affect that
route; remote sloped stair faces do not trigger irrelevant subdivisions. A nearby
nonprismatic boundary is rejected. Component sections are combined in2D, avoiding
a new destructive3D union/remeshing step. Actual guard sections must cover the
original exposed drop-edge bands within1e-7m².

Removing a deck fails foot support; sealing the portal fails the actor tube;
removing a guard fails edge coverage; an added4cm threshold fails lower-leg
clearance. Direct construction is tested with physical-floor construction forbidden.
These are limited continuous routes from existing stair fronts, not unrestricted
terrace roaming, highest-roof access, a door-state simulation or a native controller.

## Actual views

Ten current1920×1080 Eevee views were optically opened: four matched whole-
building before/after pairs and two portal close-ups. Seven are fresh renders at
`17798a40f`; three earlier valid baseline images are reused after exact mesh-array,
camera, material and ground-span checks, with their original render commits and
image hashes retained. An early run exposed unmatched ground-plane extents; those
outputs are rejected, with one explicit counter retained. Final pairs all use the
same10000m neutral flat ground/daylight. Blender5.2.2 LTS,16 samples, llvmpipe LLVM
software rendering; no GPU performance claim.

The upper-wing examples visibly gain protected deck outlines and real doorways;
close-ups show actual wall thickness, a flush floor and the simple retained
interior/stair geometry. Source silhouettes/heights and unrelated window axes
stay intact. Source0/3 near-view triangle counts change41282→41318,
48610→48798,81768→81796 and74394→74574. Raster/material repetition and bare highest
roofs remain visible defects; there is no new whole-city or night image claim.

Across all four sources × two programmes,25 protected components have22 accepted
portals and3 explicit unserved components. This counts resolved terrace components,
not source buildings or recipes: one original contact-connected region separates
into two physical components after removing sub-resolution whiskers. Original
aggregate terrace area still passes the independent1e-7m² gate. All75 added
closed deck/guard/coping components pass their own area/volume roundtrips.

Current inputs, renderer receipts, hashes and `test/scripts/pixels.py` comparisons
stay in/tmp. The code changes after physical verification are only the fuller
alternative-comparison driver and the fixed ground setting in the image probe;
physical/planning modules are unchanged from the verified revision.

## Incremental cost, not a runtime budget

`terraced_terrace_cost.py`, frozen `16d1bcf2c`, Linux/x86-64/main `.venv`: eight fresh
workers, each variant with three unprofiled fresh-owner constructions and a separate
Memray1.20.0 replay (Python/native allocation requests, no native stacks). Source
acquisition/imports/mass planning are excluded; logical preparation and direct
construction are included. Near rows additionally retain the complete physical
product. Both variants own their coarse product. New decks/guards/portals add
quality, so these are incremental context rows, not equal-quality speed comparisons.
Host load includes verification; no CPU/GPU/device frame admission follows.

| Source / programme / owner | Median ms before → after | Resident NumPy MiB before → after | Allocation requests before → after | Tracked heap peak MiB before → after |
|---|---:|---:|---:|---:|
|0 cascade direct|328.6 →385.9|1.458 →1.524|1050751 →1526024|5.516 →5.593|
|0 cascade +near|939.5 →1107.5|6.702 →6.758|3557492 →4032123|14.753 →14.855|
|0 wings direct|342.7 →437.9|1.696 →1.878|1239021 →1522489|6.503 →6.474|
|0 wings +near|1075.8 →1107.7|7.905 →8.065|4035252 →4313303|17.515 →17.753|
|3 cascade direct|498.7 →626.1|2.897 →2.981|1961382 →2241263|10.775 →10.781|
|3 cascade +near|1785.6 →2359.3|13.177 →13.251|6734655 →7016168|28.342 →28.471|
|3 wings direct|631.6 →811.9|2.632 →2.787|2061276 →2717067|10.160 →10.214|
|3 wings +near|1770.3 →1901.7|12.132 →12.266|6394599 →7047590|26.829 →27.011|

Resident bytes include retained raw/collision and presentation arrays; distinct
NumPy buffers are counted once. Source WKB/IDs/Python objects are outside that
payload but participate in the separate heap trace. Cumulative allocated traffic
is114.476–705.411MiB across these variants; it is not copied bytes or resident
memory. Whole-worker RSS peaks128.801–168.551MiB include both variants, imports,
normal and profiled work; do not add them to traced heap/output. Original16 traces
and stats are retained in/tmp. Total copies, GPU buffers and device times remain
unmeasured; no acceptable full-sandbox budget is inferred.

Logical preparation still performs unnecessary repeated profile work and world
window conversion, and near demand still builds the whole physical building.
Early empty-setback rejection, conversion of only relevant portal axes and
selected reusable near components are concrete next cost levers. Roof access,
full988-source selection, richer facade/roof variety and persistent/native demand
remain open; this local successful component does not close #618.

## References and reproduction

The local [SIGGRAPH2006 building paper](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
p.4/Fig.8 and component/scope discussion, was optically opened again. Existing
SDK3.4.12206 Ghidra offset references were rechecked against core binary SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`:
wrapper at0x55b090 delegates to CGAL helper0x671880. They inform primitive
composition, not a ready Esri terrace recipe or a fresh full-CGA oracle comparison.
Reference binaries/decompiled material stay outside the repository.

Use `.venv`, `TMPDIR=/tmp`, `PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache`,
`XDG_CACHE_HOME=/tmp/outshine-buildings-cache` and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`.

```sh
.venv/bin/python -m unittest -v verify_terraced_mass verify_terraced_mass_rooms verify_terraced_facades verify_terraced_facade_access verify_terraced_facade_products verify_terraced_terraces
.venv/bin/python test/experiments/buildings/terraced_terrace_contacts.py --output /tmp/outshine-buildings-terraces/contacts.json
.venv/bin/python test/experiments/buildings/terraced_terrace_cost.py --source 0 --programme cascade --near --output /tmp/outshine-buildings-terraces/cost.json
.venv/bin/python -m memray stats --json -o /tmp/outshine-buildings-terraces/cost-after-stats.json /tmp/outshine-buildings-terraces/cost-after.bin
.venv/bin/python test/experiments/buildings/terraced_terrace_probe.py --output /tmp/outshine-buildings-terraces/models
.venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-terraces/models/0-cascade-After-geometry.json --output /tmp/outshine-buildings-terraces/renders --samples 16
```

Run source0/3 and both programmes with/without `--near` for the full cost table.
Render all ten geometry reports; use `test/scripts/pixels.py` for matched PNGs.
Research images, traces, logs and inputs remain exclusively/tmp. No specialist
server/gallery, native changes or additional world-input acquisition is included.
