# Repeated industrial roof sections

Owner: [#480](https://github.com/Weichwolf/outshine/issues/480).

[Instant Architecture, SIGGRAPH 2003](../siggraph/2003-instant-architecture.pdf),
pages 3 and 4 optically inspected: Figure 4 separates attributed layout and
material/geometry derivation; Figure 5 illustrates repeat/subdivision and
component replacement. This supports composition from bounded primitives,
not a claim that the paper supplies this industrial recipe.

Fresh bounded Ghidra exports of SDK3.4.12206, core SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`:

- `Processor::roofShed(RoofValueType,double,double)`, `00526480`,39 instructions:
  rounds the selected edge, dispatches height/angle mode to `roofRidge`, and
  handles a negative angular orientation. The kernel and indirect work were
  not freshly re-exported here; exact generated UV/topology is not proved.
- `splitXYZ`, `004f0900`,1588 instructions: mesh vertex/edge/face cut dispatch,
  texture-coordinate storage and recomposition, not a ready folded-roof recipe.

The unchanged bounded `esri_ghidra` exporter ended0 via the installed direct
headless launcher. Originals/JSON/logs remain outside the repository in
`/tmp/outshine-buildings-folded-roofs-esri-472a8e567/`.

Our own `RoofSection` describes a continuous metrical piecewise-linear run/
height function in an ENU XY frame. Batched GEOS strip intersection/CDT preserves
the original polygon and holes; the existing heightfield primitive closes the
surface to a reference solid. Adjacent intervals share their exact cut values.
Outer intervals extend beyond the admitted domain instead of clipping tiny
roundoff slivers at a nominal endpoint. This is not epsilon mesh welding.
UVs develop cumulative section length and transverse metres; geometric normals
and interval ownership are emitted. Analytic volume, oriented edge closure,
actual facet/UV lengths and courtyard coverage have independent controls.

The inferred ordinary-industrial programme has2..8 repeated bays with symmetric
or asymmetric peaks; supplied roof fields and known special structures exclude
it. Height is reserved below the unchanged provider top before facade/room
planning. Previous nearby generic rooms retain their IDs and sampled physical
access; a bad local roof retains the previous functional flat control. A roof
prototype that discarded those rooms was rejected before delivery.

Equal scalar and batched full array/attribute outputs were compared on all
three real source domains. Both clip/CDT the same intervals; batching reduces
Python-to-GEOS dispatch, not the number of geometric elements. Cost/peak/copy
scope and current image results are recorded in the experiment README and PR.
No ready Esri industrial-recipe equivalence, glazing/clerestory, complete hall
programme, controller/night/native/device or full image/shadow LOD admission.
