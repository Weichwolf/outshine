# Requested independent ground components

Owner: [#1754](https://github.com/Weichwolf/outshine/issues/1754), Parent #434.

The geometric query driver previously stopped at the complete113-source product:
ten eligible ordinary sources remained in the requested context, with no actual
old endpoint/profile roots. The existing global independent seed selector (#561)
already handles unattached components. This change adds request bounds to that
selector and uses it when the geometric query has no attached recipes.

The existing attached result is returned unchanged whenever it has a recipe.
This does not retry independent seeds after three failed attached attempts.
Both families retain the existing three physical attempts, eight whole sources
per module, solver/terrain/body bounds and independent saved-product reader.
No artificial profile port, routing connection or native admission is introduced.

## Selection before geometric work

The existing local query terrain context supplies a conservative XY rectangle.
Cached, unsupported-level/class and degenerate source reasons retain their order.
For remaining sources, compare the original whole-axis bounds expanded by the
profile reach with this request. Reject disjoint bounds before building a GEOS
corridor or probing any old asset. For survivors, use the same actual full-width
band, flat caps and limited mitre joins for exact request intersection and the
existing complete held-terrain coverage check.

The band may intersect the request through its middle or join. Never clip the
original source to the request. After selection, probe the entire source band
against every old asset, including old ground outside the requested rectangle.
Unknown contacts and exhausted probe budgets remain explicitly deferred.
The prior65536-query/one-million-facet bounds and D/Q guard remain unchanged.

For half-width r and mitre limit2, the axis AABB reach is sqrt(5)r plus a small
Float64 coordinate guard. GEOS first accepts an unclipped mitre within2r, otherwise
intersects offset lines with a bevel segment whose midpoint is2r from the corner
and whose half-length is r. Its endpoints can therefore reach sqrt(5)r. Expanding
by only2r incorrectly drops actual150°/175° bevel vertices; the checks retain
those counterexamples and cover nearly reversing axes. Intersections and union
cannot extend the mathematical polygon beyond its generating segments.

Primary implementation: GEOS3.14.1
[addMitreJoin/addLimitedMitreJoin](https://github.com/libgeos/geos/blob/3.14.1/src/operation/buffer/OffsetSegmentGenerator.cpp).
The algorithm review first consulted Chen et al., SIGGRAPH2008,
[Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page2/Figure2, inspected visually. Its graph/physical-geometry separation informs
the existing pipeline; this packet uses no tensor-field street reconstruction.

## Evidence and reproduction

`stair_seed_demand_checks` covers whole long sources, corridor-only request
intersection, mitre extrema, zero old-body work outside demand, old contacts
outside demand, cached/unsupported sources, malformed request bounds and zero
seed work for a valid attached recipe. Run it with the existing seed/frontier,
height, closed-source, dependency and crossing check modules:57 checks pass.

Freeze sources, then run `stair_endpoint_frontier_study --resident ...
--prior-module ... --query ... --output /tmp/outshine-.../outshine-auto`.
The versioned `stair-endpoint-frontier-query.json` supplies only original-frame
geometric demand, no chosen source or cached port. On the complete113-source
prefix, recipe0 adds one whole original source; generator and fresh reader pass.
The entire previous107-source attached selection, including all recipes and
witnesses, is exactly equal to the pre-change driver and performs no seed work.

`stair_seed_demand_cost --resident ... --prior-module ... --query ...
--study ... --output /tmp/outshine-.../outshine-cost` compares early rejection with
the unchanged complete seed candidate pass followed by actual request filtering.
Both produce the same ten eligible whole sources, ten roots and twelve physical
recipes/profiles. The timed region is candidate eligibility on already held
immutable inputs; graph discovery, loading, generation and consumers are outside.

Nine alternating normal-priority rounds, Linux/i5-1135G7, Python3.13.5,
NumPy2.5.4, Shapely2.2.0/GEOS3.14.1, one BLAS/OMP thread, with concurrent native
build/lint (load average14.87/12.15/7.97), give:

| Candidate pass | Early request | Full pass, late request |
|---|---:|---:|
| Headers examined | 1657 | 1657 |
| Numeric request tests / early rejects | 645 / 620 | 0 / 0 |
| Eligibility band builds | 25 | 645 |
| Old-probe band builds | 18 | 396 |
| Late request band builds | 0 | 249 |
| Old-asset queries | 400 | 8037 |
| Candidate old facets | 118 | 1007 |
| CPU p50 | 65.655ms | 385.717ms |
| Wall p50 | 74.801ms | 421.933ms |
| Positive allocation events | 199843 | 1170134 |
| Requested allocation bytes | 16712419 | 102619616 |
| Scoped heap high watermark | 663587B | 768022B |

Memray1.20.0 captures Python allocators and native traces. Requested bytes are
neither retained nor copied bytes. Both paths still make645 source-axis copies,
37584 known bytes, during their shared semantic/degeneracy checks; total copying
is unmeasured. Held source/assets are common inputs and not added again to heap
peaks. Early outside-request reasons may take precedence over later old-contact
or terrain reasons; eligible sources and physical recipes remain identical.

`stair_seed_demand_study --resident ... --prior-module ... --study ... --output
/tmp/outshine-.../outshine-display` exports actual contact before/after and whole
scenes. Render contact scenes with `render_transport_research`; use the whole
scene for the unchanged Buildings/Plants display consumers. Open actual PNGs
and compare with `test/scripts/pixels.py`. Diagnostic family materials do not
constitute native PBR/Places acceptance. Outputs remain private under system temp.

The qualified114-source module preserves all28 previous body assets,193 arrays
and982785 array bytes exactly. Generator8.202s/145956864B peak RSS; independent
reader10.475s/172707840B, under concurrent work. These whole-generation/reader
times are separate from candidate selection and are not frame/device claims.
Source/world/routing authority, worldwide coverage, generic regional physical
contract #717, native implementation, UMA reserve and both devices' p99 remain
open. The Outshine research goal continues after this packet.
