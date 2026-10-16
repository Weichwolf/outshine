# Stepped physical facade presentation

Owner: [#460](https://github.com/Weichwolf/outshine/issues/460).
This delivery forwards existing verified geometry; it introduces no Esri-equivalent
window recipe or new Boolean implementation.

## Primary and local binary references

[CGA Shape, SIGGRAPH 2006](../siggraph/2006-procedural-modeling-of-buildings.pdf),
pp. 3–4 optically reviewed: component surfaces, facade detail and composed solids.
[Mesh Arrangements, SIGGRAPH 2016](../geometry/2016-mesh-arrangements-for-solid-geometry.pdf),
p. 1 optically reviewed, including Figure 1: an arrangement/extraction algorithm
must preserve the intended boundary and cell classification. Our existing
Manifold-based procedures are not that paper's exact arrangement algorithm.

Existing local Ghidra release exports were inspected and their individual C-file
hashes checked against `ghidra-core.json`:
`build/experiments/buildings/esri-boolean-ghidra-release/`.
Pinned external SDK core SHA-256:
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.

- `Processor::__boolOp`, Ghidra `00540ca0`, 70 instructions: selector/swap dispatch
  into `calculateBoolOp_OpSeqs`. This is dispatch evidence, not a complete kernel.
- `Processor::subtract`, `00541230`, 243 instructions: operation dispatch and
  diagnostics. Unresolved/indirect calls and decompiler types remain unverified.
- Existing split/construct/cleanup exports document their own bounded call scope;
  they do not establish a Python/Esri mesh or automatic room-planning oracle.

## Rejected reconstruction paths

An additional whole-solid window-pocket subtraction using the existing
`subtract_prisms` rejected the real oblique courtyard source because serialized
triangles had unresolved normals. A local planar union/difference/CDT alternative
produced the expected pocket volumes, but did not retain the original edge/root
relationships adequately. It was not integrated. Position-only welding is also
an invalid general oracle: the original second source has separate coincident
component relationships. These observations do not invalidate its existing
root-aware physical construction or justify dropping triangles/welding by epsilon.
The fixed original source inputs from #442 remain versioned.

## Chosen procedure and limits

The successful plan validator already builds and checks the complete actual near
assembly, including frames, sills, openings, floors, stairs and simple rooms.
`plan_and_near` forwards that very result, then checks the completed portal graph.
The consumer shares its positions, indices and normals and supplies presentation
UVs/material bindings. Removing a second construction is both simpler and
geometrically stronger than reconstructing another approximated facade.

The component-resolution criterion uses a conservative 0.25 m radius and the
existing 0.5 px threshold. It is not a whole-interior/shadow/image LOD bound.
Near products draw substantially more triangles than compact hulls. Existing
opaque procedural glass presentation does not admit transparent/night interiors.
Native interaction, visibility/cost admission and broader architecture stay open.
