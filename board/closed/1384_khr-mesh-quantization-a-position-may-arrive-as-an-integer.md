Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr mesh quantization a position may arrive as an integer**

Positions, normals, tangents and texture coordinates may use integer component types the base format
forbids for those attributes. **It is structural** -- it changes how vertex data DECODES, which is
upstream of every material question -- and it is the compact form most shipped assets use.

**One model at the pin requires it.**

- [x] The accessor decode is where this lives, not the draw path -- it already was, generically, which is exactly why the CHECK is the behaviour
- [x] Normalised and unnormalised integer forms are both decided, and a test says which is which
- [x] The bounds a case's camera is derived from are computed AFTER the decode, or the framing rule
  frames integers

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_mesh_quantization>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).

## Comments

**The decode already accepted every component type, and that is the defect this closes.** A reader
that draws a quantised POSITION without being told the file is quantised is not implementing the
extension -- it is failing to check -- and the file it draws wrongly is the one that quantises WITHOUT
declaring, which the extension forbids precisely because *the extension does not provide a way to
specify both FLOAT and quantized versions of the data*. **So the check is the behaviour.**

**[MEASURED] before the code was written: the enforcement refuses none of the 148 models.** One
declares the extension and uses it; no model uses a non-base combination without declaring it. *That
number is why a validation this strict could land at all, and taking it first is what turned a risky
change into a safe one.*

**It broke two of our own tests and both were right to break.**

| test | why | ladder rung |
|---|---|---|
| `AGlbCarriesWhatItDeclares` | its fixture carries a `byte normalized` NORMAL, which base glTF forbids -- **it was never a legal glTF file** | patch the asset |
| `AFileThatCannotMeanAnythingIsRefusedByName` | it used `KHR_mesh_quantization` as its example of an unimplemented extension | **its SUBJECT moved**, which is the one legitimate reason to edit a specification |

*Neither was adjusted to go green: the first file became legal, and the second now names an extension
this reader genuinely does not honour.*

## Measured, and the corpus did not move -- which is what the pre-measurement predicted

Trailer before and after: **327 PASS, 108 FAIL, criteria 126 of 133, 115 within the bound, 36 red.**
Byte-identical, no case either way. **That is confirmation rather than a mystery**, because the census
taken BEFORE the code said the enforcement refuses none of the 148 models. *An identical result is a
finding when it was not predicted; here it was the prediction.*

**The one model that requires the extension is `MeshoptCubeTest`, and it is still not scored.** The
reader now reads it; its PREPARATION refuses, on the identity-quaternion shape recorded under
`board:1375`. **So this item unblocked the reader and not the model**, and saying otherwise would
claim a case this did not buy.

*It also carries `KHR_meshopt_compression`, which it declares as USED and not required -- so that is
not what stops it either.*
