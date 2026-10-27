# Stairs between chosen floors

Bounded Python follow-up to [indoor floors](indoor_floors.md),
[#1553](https://github.com/Weichwolf/outshine/issues/1553). An actual source axis
connects two explicitly chosen metric floor heights through closed stairs,
whole landings and a checked opening in the upper plate. All30 earlier XYZ
contacts remain exact; all73 ready corridors from the expanded85-way query
remain covered after the opening. Equal-height stairs stay pending.

## Inputs and contracts

The same pinned three complete vectors and one DEM supply219 indoor axis parts,
including64 staircase sources. `indoor_stair_contacts.py` binds candidates from
exact normalized rational MVT grid coordinates, including interior source
vertices. The provider adapter now retains each layer's actual extent.
One unambiguous chosen floor/height is required at each end. This establishes
procedural contact candidates, not original OSM node identity or native World
topology. Labels0/1 do not determine metric rise.

The13 bound end-height pairs comprise ten nonzero and three equal-height
sources. Ten nonzero sources have nine geometrically different axes because
one is duplicated in a neighbouring tile buffer. Candidate tests are independent
against the same floor context; they are not a jointly qualified stair network.
The driver emits only the explicitly requested source and records every other
source's candidate result and omission reason.

## Construction and checks

`indoor_stair_recipe.py` enumerates a finite family of balanced straight flights.
Explicit recipes: rise0.14–0.18m, tread0.25–0.35m, `2*rise+tread`0.60–0.66m,
1.5m end/intermediate landings, at most16risers/flight,64risers/eightflights.
Prefer fewer flights, then closeness to nominal rise/stride and fewer risers.
This is not arbitrary-layout optimality or a building-code certificate.

`indoor_stair_geometry.py` reuses the existing prism builder on a stair section,
then maps the extrusion into the declared ENU frame. Sloped soffits maintain
at least0.2m vertical waist thickness: merely translating the stepped top would
pinch the waist at the risers. A real1mm side construction margin gives1.502m
body width around the requested1.5m corridor. Check emitted triangles for
oriented closure/positive volume and actual horizontal tread domains, retaining
face ownership. Independent profile sections check minimum thickness.

`indoor_stair_site.py` requires both complete landings on the chosen host floors.
Intersecting headroom prisms create explicit1mm-expanded plate openings.
An opening may not remove any ready corridor. Rebuild and check the actual
plate mesh; the resulting slabs must clear every tread's2.1m headroom.
Per-tread shared `Reservation`/`HeightInterval` products, a `SourceFeature` and
the explicit batch frame preserve space for later room/door planning.

Source volumes describe a chosen compound envelope, not filled solid obstacles.
Internal part planes cancel in its3D union. For every vertical stratum between
source height events, union the active footprints; require the whole stair-body
and headroom prism, plus a0.2m recipe roof reserve, inside every crossed band.
Gaps, exposed undersides and insufficient roofs remain hard failures. The
existing floor reference keeps its more conservative band/plane selection;
its selected physical slabs remain explicit obstacles here. The new union
interpretation does not delete an emitted beam or plate.

The repeated reference reconstructs all bands per tread/landing. The retained
`bands` procedure constructs them once. Identical bodies, openings, plate meshes
and reservations must result. Report unions/input-parts/coverage tests alongside
CPU, allocation traffic, scoped peak and process RSS. Source read/decode/ENU/DEM
and expanded floor construction are measured separately; these are not finished
asset-cache hits. No total-copy or device-frame measurement is implied.

## Reproduce

Same dependencies as the floor reference; Matplotlib for local diagnostics,
Memray for optional allocation originals. No new packages. From repo root:

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest discover -s test/experiments/buildings -p 'test_indoor_*.py' -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/compare_indoor_stairs.py --source openfreemap:14/8939/5680:transportation:4040280470:2 --output /tmp/outshine-research-indoor-stairs-comparison --rounds 3 --trace --plot
```

The output directory must be new. JSON contains original sources, chosen end
profiles, construction/qualification limits, final floor models, reservations,
all64 independent candidate outcomes/omissions and separate cost scopes. NPZ
contains actual stair/final plate buffers. Local diagrams show actual side
triangles and plate sections before/after the opening, plus the actual body.
The uncut candidate is obstructed and never released as ready. Images stay
in system temp; no repository image publication.

Primary reference: Merrell, Schkufza and Koltun, SIGGRAPHAsia2010,
[Computer-Generated Residential Building Layouts](../../../doc/references/buildings/siggraph/2010-computer-generated-residential-building-layouts.pdf),
section6/page7–8 and Fig.11, text and actual pages inspected. It motivates
stair-space/shape-dependent construction and the need to reject blocked upper
access. The finite schedule and stratum-union proof here are our derivation;
we do not use the paper's stochastic floor-plan optimizer or assert its style
templates as Outshine requirements.

Scope remains chosen local source-snapshot geometry. Whole-terrain support,
statics, rails, walls/doors/access, jointly qualified multiple stairs, original
OSM topology/native Source→World integration and A18/Iris/p99 are open.

The [joint follow-up](indoor_stair_batch.md) composes multiple sources/plates.
It retains the individual procedure, adds explicit candidate staging and final
whole-width endpoint checks, and shares source/floor preparation. Landings are
stair solids; after cutting, the final host must cover the complete endpoint
interface, while the pad can bridge its own opening guard. Joint geometry still
does not supply rooms, access or native World qualification.
