# Joined facade hierarchies

Owner: [#474](https://github.com/Weichwolf/outshine/issues/474).

[Instant Architecture, SIGGRAPH 2003](../siggraph/2003-instant-architecture.pdf),
p. 3 optically inspected: attributed split/control grammar, facade-to-component
derivation and architectural hierarchies. [CGA Shape, SIGGRAPH 2006](../siggraph/2006-procedural-modeling-of-buildings.pdf),
p. 5 optically inspected, Figures 9–11: alignment/snapping and bounded
occlusion reasoning. Our programmes reuse existing source/window/storey axes;
they do not reproduce the entire CGA or native occlusion system.

Fresh bounded Ghidra exports on local SDK3.4.12206, core SHA-256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`:

- `Processor::extrude(double)`, `004b5210`,3194 instructions: `HoleRelation`,
  side/top tagging, source-tag copying and `setSideFaceTexCoords` calls observed.
  Exact optimized connectivity, inferred types and indirect callees remain
  unverified; these calls do not establish an executable facade-frame oracle.
- `splitXYZ`, `004f0900`,1588 instructions: existing mesh-cut/scope dispatch,
  not a ready residential or mixed-use facade recipe.

Both headless exports ended0 using the direct installed Ghidra launcher. The
versioned `esri_ghidra` bounded symbol/body exporter was unchanged; only these
two targets were selected. Originals/JSON/C/logs remain local under
`/tmp/outshine-buildings-facade-frames-esri-8a429c34c/`; no binary/foreign source
is bundled. SDK correspondence is limited to inspected primitive mechanisms.

Our frame front is the existing polygon/holes triangulation plus explicit
boundary sides, with no separate overlapping cross-members. One plan supplies
closed collision-reference and covered-back outer products; the latter never
builds its hidden back. Exact edge direction, analytic volume, actual aperture
rays, normals and metric UVs are checked. Physical window/casing/sill bounds
come from the actual existing window module, not a second inferred opening.
Known neighbor-height/source-wing negatives are independent of the camera.

The bounded broadcast region beat meshgrid and GEOS bar union at the same
measured polygon contract in21 runs on both actual lattices; GEOS native heap
is excluded from traced peaks. Closed solids were also checked independently
against3D bar union, whose extra provenance and missing render UV/normals make
full attribute pipelines unequal. Counts, times, peaks and limits are in the
experiment README; local full reports remain under the issue's linked PR.
Frozen `12fec1ac8` passed63 source and63 fresh-wheel tests,80 packaged building
modules byte-exact. Actual sections, 2.5D and1920×1080 Blender component/city
comparisons were opened. The988-source city adds3814 triangles (+1.017%);
nearby grouping improves but city-wide architectural monotony remains.
No entire Esri recipe, complete architecture family, night/native/controller/
device or full image/shadow LOD admission follows from this component research.
