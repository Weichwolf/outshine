# Contained roof crown references

Owner: [#551](https://github.com/Weichwolf/outshine/issues/551), exterior parent #32.
This is an own roof programme composed from existing polygon/heightfield operators,
not an Esri architecture recipe or a claim of SDK equivalence.

## Primary sources

[CGA Shape, SIGGRAPH 2006](../siggraph/2006-procedural-modeling-of-buildings.pdf),
p.4 optically opened at this work block: Figures5–8 distinguish mass vocabulary,
roof forms and composed shell surfaces. The adjacent text separates source/lot
containment, solid composition, component scopes and context occlusion. The
present experiment partitions the supplied plan first and composes its upper
heightfield before facade/opening layout. It does not infer a measured roof plan.

Local SDK HTML under `~/Downloads/esri_ce_sdk-3.4.12206/doc/html/cgaref/cgareference/`:

- `op_offset.html`: negative distances construct interior contours; inside and
  border components partition the source face. Face orientation is explicit.
- `op_innerRect.html`: the operation seeks the largest contained rectangle aligned
  to scope axes. Our finite candidate search is deliberately a different bounded
  placement procedure; neither maximality nor the SDK's algorithm is claimed.

## Bounded Ghidra inspection

External core SHA256:
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
The existing symbol-bounded exporter completed with exit0 using the direct Snap
Ghidra launcher and bundled Java21. Project, logs and exports stayed under `/tmp`.
Exports: `/tmp/outshine-buildings-crown-esri-2be894e5e/ghidra-core.json`.
The exporter was modified for temporary paths at inspection time; its recorded
dirty producer status is retained, rather than represented as a clean snapshot.

| Ghidra address | Observed function | Instructions |
|---|---|---:|
| `0055b090` | `PolygonOps::computeOffset` dispatches to `CGALTools::computeOffset` | 63 |
| `00671880` | `CGALTools::computeOffset` includes CGAL straight-skeleton builder calls | 1905 |

These are bounded dispatch/control-flow observations. Unresolved calls remain;
decompiler types and complete symbol-body coverage are unverified. Multiprecision
and straight-skeleton symbols support the primitive's implementation route, not
exact floating-point matching, general topology proof or a ready parapet family.
The Python implementation uses GEOS mitred offsets, constrained polygon
triangulation and the existing closed heightfield envelope.

## Own programme and limits

An inward offset separates coping, a steep inner guard face and the flat deck.
An optional contained rectangle adds a solid service volume with a narrow bevel.
All heights lie below the delivered top; the lower deck datum precedes facade
layout. Proportions, service use and programme choice are explicitly procedural.
Courtyards remain void. No roof access, service interior, weatherproofing,
drainage engineering or measured architecture reconstruction is implied.

The coarse product merges coplanar regions by material and retains the same2D
material coverage on a flat top. Its geometric displacement is bounded by the
reserved roof rise. That does not establish a universal radiance, shadow, night,
temporal, cache, native or device result; actual image comparisons remain required.
