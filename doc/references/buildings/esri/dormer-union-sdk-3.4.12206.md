# Attached roof volumes: union and extrusion observations

Owner #585, parent #32. These are bounded primitive observations, not a recovered
Esri dormer recipe, complete modelling source or numerical equivalence claim.

SIGGRAPH2006 *Procedural Modeling of Buildings*, page3/Figure4 and the mass-modelling
transition were rasterised and optically inspected from the local
[paper](../siggraph/2006-procedural-modeling-of-buildings.pdf). Component scopes,
extrusion and composition motivate our attached roof volumes. The paper does not
prescribe our box dimensions, eligibility, source interpretation or cavity depth.

Local SDK3.4.12206 Linux core `libcom.esri.prt.core.so`, SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Fresh Ghidra12.1.2 headless/Java21, x86 LE64, image base `0x100000`, terminal0:

| Function | Ghidra address | Recovered instructions | Bounded named observations |
|---|---|---:|---|
| `Processor::_union(int,int)` | `00541690` |243| `calculateBoolOp_OpSeqs`, operation-name/signature handling |
| `Processor::extrude(double)` | `004b5210` |3194| inverse-transpose matrix, point transforms, normalisation, face materials and side-face UV helper calls |

Union dispatch passes to the named boolean-operation sequence helper. This does not
reveal or validate that helper's geometric kernel or every operand/tagging rule.
The extrusion decompilation contains numerous unresolved direct targets. ELF symbol
sizes1109/17373bytes, nominal Ghidra bodies1094/17744bytes and recovered control-flow
coverage1059/16808bytes differ. Body/type/branch completeness remains unproven;
the figures are not a claim that all extrusion or union semantics were recovered.

Our experiment reuses the existing own Manifold-backed prism encoding/decoding
operators to unite real roof meshes with bounded source-contained box volumes.
It explicitly keeps triangle/material/source provenance and metric front UVs.
An independent integral over the original affine roof triangles predicts added
volume; source hulls, courtyard holes and original top height remain constraints.
Near window pockets use the existing prism subtraction operator, existing metrical
frame templates and explicit glass surfaces. Matching body/roof products are cut
from the same closed result, rather than stacked overlapping hidden cubes.

No external code, decompiler export or binary is bundled. Actual private exports:
`/tmp/outshine-buildings-dormer-esri-1db54e3aa/ghidra-core.json`,
`reference-00541690.c`, `reference-004b5210.c`; terminal summary:
`/tmp/outshine-buildings-dormer-ghidra.log`. Python/image/function/cost evidence is
owned by #585, independently of this SDK observation.
