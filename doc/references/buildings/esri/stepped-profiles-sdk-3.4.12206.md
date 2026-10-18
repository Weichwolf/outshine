# Upper-storey profile references

Owner: [#442](https://github.com/Weichwolf/outshine/issues/442), architecture parent #32.
This is a bounded Python recipe extension, not an implementation of Esri envelope/setback semantics.

## Primary sources

[CGA Shape, SIGGRAPH 2006](../siggraph/2006-procedural-modeling-of-buildings.pdf),
pp. 3–4 optically reviewed: explicit solid composition, source/lot containment,
component surfaces and context/occlusion. Figure 5 distinguishes actual mass
configurations; seeds and instantiated buildings are not architectural families.
The present extension changes upper-storey contours while retaining the existing
connected generic room/core plan and actual access checks.

Local SDK HTML under `~/Downloads/esri_ce_sdk-3.4.12206/doc/html/cgaref/cgareference/`:

- `op_envelope.html`: edge-dependent base heights and sloping planes, capped by
  maximum height; street-side selectors fall back to object selectors.
- `op_setbackPerEdge.html`: setback expressions evaluated separately per face edge.
  These are polygon operations, not evidence of automatic room or terrace planning.

Our orthogonal/diagonal half-plane recipes clip the supplied footprint, preserving
its holes and lower outline. They do not interpret provider geometry as a measured
upper-storey blueprint. Explicit procedure metadata distinguishes the inferred form.

## Fresh Ghidra evidence

External core SHA-256:
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Existing bounded exporter `esri_ghidra.py`, targeted independently at:

| Ghidra address | Observed dispatch | Instructions |
|---|---|---:|
| `005062d0` | `Processor::setback(double, SplitNode)` → `innerSetback` | 42 |
| `00506470` | per-edge double array → `checkArray`, `innerSetback` | 97 |
| `00506630` | per-edge array plus scalar → `checkArray`, `parseUvAndSetback` | 98 |

Headless completed with exit 0. Snap's launcher failed creating its temporary root;
the installed `/snap/ghidra/current/ghidra/support/analyzeHeadless` and bundled
Java 21 ran directly. No extra package or SDK source was acquired. Local report:
`/tmp/outshine-buildings-corner-esri-45ef114aa/ghidra-core.json`.

Helper exports also complete with exit 0: `innerSetback` (`00510a60`, 11
instructions) delegates to `SetbackOperation::setback`; `parseUvAndSetback`
(`00510cd0`, 70 instructions) bounds a converted selector to 0–9 and reports
a CGA error outside that range before dispatch. The numerical rounding
constants and the actual offset algorithm remain unverified. Helper report:
`/tmp/outshine-buildings-corner-esri-helper-45ef114aa/ghidra-core.json`.

These are dispatch observations. Array copies have unresolved calls; shared-owner
cleanup and decompiler excursions outside the bounded symbol are not algorithm
proof. No exact floating-point, room, collision, or Python/Esri mesh equivalence
is inferred. Decompiled foreign code stays outside the repository.

## Intended checks

Independent metric rectangles/corner triangles and rigidly moved holed footprints
check the requested contours. Existing setback/terrace construction retains the
core and room identities, and rejects lost rooms or inaccessible terraces. Actual
near geometry and closed exterior sections decide admission; otherwise the old
profile remains available with the reason for fallback. The source fixture contains
four real provider footprints and supplied properties; it is not place-specific
replacement geometry. More than two additional profile classes are not claimed.
