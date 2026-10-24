# Native landing masks and conforming sections (#1189)

`BuildSurfaceLayout` takes bounded source domains, axes, widths and declared
landing incidence. Source ownership and order are explicit. It constructs landing
masks, clips interior transverse sections to their own domains, and produces one
conforming arrangement with source masks. It neither infers a connection from
overlap alone nor assigns heights. Steps use the separate flight/station planner;
geometric path sections are not additional parent-road height equalities.

The existing Python recipe supplies the comparison: an eight-sided disk of radius
half the maximum incident width, nearby pairwise domain intersections, and optional
landing reach. Interior sections follow source station distance, clipped around
holes. Default spacing is 2 m; it is a recipe parameter, not an OSM requirement.

Clipping followed by overlay can leave a nominal endpoint just inside its source
boundary. Area checks alone miss the resulting dangling section. Working section
ends extend collinearly by the existing arrangement weld bound (default 1e-8 m);
reported cuts, domains and supplied native profile vertices remain unchanged.
After arrangement, a spatial index verifies each section is covered by edges
belonging to its source within that same bound. Missing cuts, exceeded budgets and
cancellation publish no layout. A bounded coverage check also distinguishes
rounding at an axis endpoint from an axis crossing a genuine hole.

Source coordinates, incident pairs, sections, output coordinates and candidate
edge tests have explicit limits/counters. Defaults permit 64 sources/landings,
4096 input coordinates, 256 sections per source and 1024 clipped parts. These are
local proposal limits, not limits on world content. GEOS overlay/triangulation and
index construction are bounded by those inputs; no global linear-time claim.

## Reproduction

```sh
make suite SUITE=outshine/src/generators/road/SurfaceLayout JOBS=2
PYTHONPATH=lib/src python3 test/experiments/transport/native_surface_layout.py \
  --output "${TMPDIR:-/tmp}/outshine-surface-layout-<unique-run>"
```

Requires NumPy and Shapely with `coverage_is_valid`. The experiment records versions
and builds its native probe with current compilation-database flags. Original,
translated and rotated actual source domains check the unchanged Python masks/
sections, conforming coverage, holes, source ownership and all 32 fixed native
outer-profile XY vertices. Rotation is a regression for silently lost sections.
Module tests cover analytic area, split sections around holes, incidence, invalid
inputs, capacity and cancellation. No oracle is regenerated.

This is a 2D producer, not world admission. The subsequent height program must
respect its exact clamped-affine flight parameterization: flat landing facets
have identically zero monotonic derivatives, so they generate no redundant
gradient rows. See `native_flight_plan.md`. Navigation, real DEM contacts,
cache/runtime use and visible street repair remain open in #1189. Prepared CPU
evidence proves neither device budgets nor a smaller render-LOD error bound.
