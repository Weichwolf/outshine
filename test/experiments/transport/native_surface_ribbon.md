# Native ribbons between complete source profiles (#1189)

`BuildSurfaceRibbon` replaces `mvt_native_cut_domains.cut_ribbon`: original source
axis plus two complete transverse XY profiles become a cropped axis and one source
domain. Supplied station order and axis-vertex identities are explicit. Every profile
vertex is copied unchanged; the caller retains its corresponding XYZ height and
source identity. No endpoint is inferred, flattened, rounded or closed automatically.

Interior source knots remain. Left/right offsets interpolate between the supplied
profile extremes in original source station space. For adjacent unit normals `a,b`,
the miter is `(a+b)/(1+dot(a,b))`. The existing recipe rejects denominator
`(1+dot(a,b))/2 <= .25`; sharper turns require another local construction recipe.
GEOS checks polygon validity, positive area and coverage of the constructed axis.
Errors/cancellation publish no partial domain. Missing profiles cannot become caps.

Default bounds: 256 source-axis points, 128 points per profile, 768 boundary points.
Axis/profile preparation is O(N+P); that is not a claim about GEOS validation's
asymptotic complexity. Validation operates on the same bounded local proposal.

## Reproduction

```sh
make suite SUITE=outshine/src/generators/road/SurfaceRibbon JOBS=2
PYTHONPATH=lib/src python3 test/experiments/transport/native_surface_ribbon.py \
  --output "${TMPDIR:-/tmp}/outshine-surface-ribbon-<unique-run>"
```

The experiment builds with current compilation-database flags. Existing actual
native-cut fixtures supply four complete profiles for two source parts. Original,
translated and rotated controls compare the existing Python recipe and native
construction: axis/boundary coordinates, area, miter and exact supplied profile
vertices. It records dependency versions and build-only times separately from
process startup. Tiny timings under load are diagnostic, not a device budget claim.
The native module tests additionally use analytic straight/trapezoid/right-angle
areas, retained interior profile knots, invalid inputs, sharp turns and capacity/
cancellation rejection. They do not depend on the wider transport/render engine.

This produces source domains, not landing masks, section constraints or qualified
ports. Native profile extraction, navigation, actual DEM contacts, tunnel volumes
and runtime/cache/world admission remain separate work in #1189. Domain construction
alone proves neither a visible street repair nor a smaller render-LOD error bound.
