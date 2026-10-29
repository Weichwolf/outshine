# Whole through sources at cached ground ends

Owner and integration: [#1736](https://github.com/Weichwolf/outshine/issues/1736),
under [#434](https://github.com/Weichwolf/outshine/issues/434).

An existing open Ground endpoint can now be the primary contact of a new whole
source whose unique original interior vertex is at that endpoint. This completes
the already supported secondary old-end/new-through case. The original MVT
occurrences and exact metric frame determine eligibility. The actual cached
three-point outer section determines the contact heights and width.

`stair_root_contact.py` derives the role from the original sources. Producer,
saved-source verifier, height plans, junction pads and whole-profile auditor use
that role. Old-interior roots still require a new source endpoint. Repeated
occurrences, shifted frames, closed old sources and unsupported Ground/profiles
remain rejected. The existing eight-source, pad, face, solver and contact budgets
apply. New-through side checks require positive-area owner facets on both sides,
including a root shared with another new branch. Actual old facets may only be
borrowed in the existing explicit source/mask domains.

The procedure follows the distinction between network connectivity and realized
3D geometry in Chen et al., *Interactive Procedural Street Modeling*, SIGGRAPH
2008, sections 6/6.1, figures 7–8. The local paper is
[here](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
Its page 5 was visually inspected. Our source occurrences alone do not certify
OSM node identities or an operational route; complete physical surfaces are
checked independently.

## Reproduction and current result

Use `.venv` and the existing NumPy, Shapely, Clarabel, Memray and Blender packages.
Set `PYTHONPATH` to `lib/src:test/experiments/transport:test/experiments/assets:`
`test/experiments/render:test/experiments/buildings`, with BLAS/OMP threads 1.
The fixed recipe is `fixtures/stair-through-root-vienna.json`; its original
vector/DEM input hashes are inherited from the existing reference fixture.
Resident and ordered `--prior-module` arguments are explicit saved inputs.

1. Run `stair_local_generate.py`, then `stair_local_verify.py`, with identical
   `--resident`, repeated `--prior-module`, `--fixture` and fresh temp `--output`.
2. Run `stair_root_contact_study.py` with those inputs, `--local` product and a
   separate fresh temp `--output`. It checks old asset bytes and exports full and
   root-detail scenes using the actual held bodies, terrain and supports.
3. Render its four scene JSONs with `render_transport_research.py`; open all PNGs
   and compare each before/after pair with `test/scripts/pixels.py`.
4. Run `stair_root_contact_cost.py` with the same inputs, `--local` product and a
   separate fresh temp `--output` at normal priority. It retains two Memray traces.

On producer `14249b63a`, complete source `80922970:255` (149.750 m) extends the
107-source context to 108. Root `39881120:19`, end 0, binds new original vertex 1.
The new top has 21 vertices/20 faces and the closed body 80 faces. Its D/Q volumes
are 80.360403/80.360633 m³ and normalized maximum grades 0.563868/0.563840.
All 27 old assets/186 arrays/957,042 bytes remain exact. The old section and both
new through sides pass complete D/Q trace checks. Both new outer ends stay open.
All 124 previous support contacts hold. The existing `free_region` terrain recipe
replaces 194 facets with 107 points/172 facets, preserving 40 boundary vertices.
The legacy layout rejects this same body for invalid terrain coverage.

Measured on `b0bddf6dc`: nine rotating held-input rounds, Python 3.13.5, NumPy
2.5.4, Memray 1.20.0, load 14.78/13.52/9.81. Legacy/free-region CPU medians are
120.818/129.710 ms; allocation events 421,805/457,783; requested bytes
53,395,968/57,972,748; scoped peaks 637,263/559,636 bytes. Legacy fails and
free-region exactly reproduces the accepted saved layout: these are different
validity outcomes, without an equal-result speed claim. Polygon difference inputs
are 194/1. Reported raw/unique/mapping and welding buffers are lower-bound work
evidence; total copies, process/UMA and native times remain unmeasured.

Controls retain actual overlap failures for `80922970:345`/`:253`, the incomplete
explicit surface portal for `:253`, infeasible grade/contact constraints for
`:242` in both existing pad recipes, the closed old `:246` root and missing DEM
context for the long service-adjacent cycleway. Source omission reasons remain.
Images include the changed terrain triangulation, diagnostic materials and open
ends. Native source admission, routing, World, device memory and p99 stay open.
