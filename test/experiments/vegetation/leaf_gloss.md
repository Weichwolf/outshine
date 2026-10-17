# Leaf gloss: explicit allocation before adding a lobe

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Follows the [source-loaded two-sided diffuse study](sheet_area_light.md).
This Python experiment compares the existing AreaMixture convention with an
additive counterexample. It changes neither the installed library nor native
shader parameters. The old diffuse renderer remains the default.

## Model and sources

First local source: Karis, SIGGRAPH2013,
[Real Shading in Unreal Engine 4](../../../doc/references/materials/siggraph/2013-unreal-engine-4-shading.pdf),
PDFpage3, visually inspected equations for GGX, perceptual roughness and Fresnel.
Its geometry approximation is not our exact implementation. Reuse Material's
`render/relief_shading_reference.specular_from_dots`: correlated-Smith,
single-scatter GGX, alpha=roughness², dielectric F0=.04/F90=1. It returns the
BRDF times incident cosine. This is not the complete native shader's energy
compensation, environment lighting or texture filtering.

Read-only Ghidra12.1.2 inspection revisited the existing local SpeedTree10.2
analysis targets `SMaterial::SMaterial` at0x21c0ca0,
`StartMaterialAssetShader` at0x1f151f0 and `PackMapToConstant` at0x218e010.
All three decompilations terminated successfully. These concern material state,
shader/texture binding and map adjustment constants; they do not establish a
physical leaf energy allocation. The supplied shader sources separate gloss,
specular/metallic and subsurface inputs. No arbitrary decompiler offset is
promoted to a physical parameter. Current installed executable SHA256:
`9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c`.
No vendor source, asset or decompilation is included in the repository.

Let S be the existing diffuse sheet response and G the black-base-colour
GGX dielectric response. Compare `A=(1-p)*S+p*G` with `D=S+p*G`.
The area mixture follows the existing [#134 convention](https://github.com/Weichwolf/outshine/issues/134).
It represents glossy absorbing patches among diffuse thin-sheet patches,
not a continuous wax coating. Both are reciprocal in the symmetric two-sided
model. Transmission is `(1-p)*T` for A and T for D. Original cached R/T bytes
remain unchanged, but A does **not** preserve their whole-leaf interpretation.
If each component has hemispherical energy at most1, their convex mixture
also does. D has no such guarantee. No pigment/albedo compensation or fitting.

Roughness.3 and p=.12 are declared author controls, not calibrated species
values or newly agreed product defaults. Parameter questions remain at
[#643](https://github.com/Weichwolf/outshine/issues/643#issuecomment-6240916670).
Opaque neighbours still block the direct beam; no BSSRDF, indirect transport,
ballistic transmission through neighbours or closed-needle tissue model.

## Independent controls and angular failure

Seven checks cover an analytic normal-incidence GGX value, the existing
Material reference, reciprocity, rigid rotation, exact opposing directions,
zero-gloss early exit, transmission weights0/.12/1, invalid inputs and an
independent one-dimensional white-furnace integral. At roughness1 and normal
view, `E_G=integral_0^1 F(sqrt((1+u)/2))*u/(1+u) du` equals.012306382500539.
For a saturated sheet R=T=.5 and p=.12, A=.881476765900065 whereas
D=1.001476765900065. Adding gloss violates energy even in this broad lobe.

The diagnostic hemisphere grid uses Gauss-Legendre cosine and uniform azimuth,
orders64/128/256, five roughnesses and seven view cosines down to.025.
At roughness.3/view cosine.4, E_G converges.107031768844352 →
.107031773808021. At cosine.025 it instead changes.334279 →.393223 →.413055.
The largest128→256 change over seven views is.137516 at roughness.2,
.019832 at.3, .005401 at.35, 3.52e-7 at.7 and1.08e-14 at1.
The uniform grid is **rejected as a general grazing-gloss energy oracle**.
These are successive finite quadratures, not certified continuous error bounds.
No larger runtime bank or product table is proposed. The analytic counterexample
and conditional convex-mixture law do not rely on resolving the narrow lobe.

Four full-sphere direct controls also agree with the independently accumulated
linear-mixture expression within5.56e-17. Their reduction uses `math.fsum`:
plain matrix reduction accumulated6.93e-14 error over32768 positive terms.
This precision correction changes only the diagnostic, not the GGX/render path.

## Actual source and images

Use the same six original open leaves/IDs, prepared pigment optics, camera,
front/back lights, beam strengths, shadows, two samples and2560×1440 resolution
from the preceding study. Prepared source NPZ SHA256:
`764aa72e8e98d8a8e72219112817978f9b386b5c69fbd51d22aded3f1bb40032`.
No whole plant, mesh expansion, optical table or photo texture is regenerated.
Four area/additive RGB images were actually opened. Gloss remains subtle;
area mixing reduces the diffuse green/front and yellow-green/back response.
A small light-coloured lobe appears on some folded surfaces. Serration, regular
folding and simple pigment fields remain visibly synthetic; no veins or
calibrated wax layer. This is not species, whole-tree or SpeedTree acceptance.

Coverage is exact across all alternatives:168137 covered pixels. Maximum
linear RGB difference from diffuse is.0109605/.0181565 for area front/back;
additive gloss alone contributes up to.00265534/.0189356. Across the full
images, `D-A=.12*S` holds within7.75e-9 after Float32 storage. Zero weight
reproduces all three original arrays and all four PNG bytes exactly.
The installed-library area replay is likewise exact for all arrays/four PNGs;
all206 installed Python module files match source bytes. No `lib/src` on its
import path. This reuses the unchanged #667 wheel, not a newly built library.

## Work and reproduction

Frozen executable experiment commit: `fd1d081f28fcb76124306bfe3bc11e2b6deedf58`.
Linux/WSL x86_64,4CPU affinity,16.58GB host RAM, Python3.13.5/NumPy2.5.4,
Shapely2.2.0/GEOS3.14.1, one numerical thread. CPU diagnostics under ordinary
shared host load; no GPU/A18/RTX or native frame-budget evidence.

Each portrait retains191692800B diagnostic arrays and uses7372800 primary rays,
334435 visible contacts,668870 light/contact evaluations and8041670 total rays;
existing ray input copies225166760B. Camera packets contain at most8192 samples.
Gloss runs only at visible contacts and skips specular arithmetic at p=0;
GGX is zero on the opposite hemisphere. Extra NumPy temporary allocations and
memory traffic are not fully instrumented. There is no extra persistent bank.
Area/additive/export wall times11.083/9.557s, process peak RSS488058880/487071744B;
zero10.414s, installed area13.166s. Different host loads prevent speed rankings.
The angular diagnostic evaluates12042240 GGX samples in8.426s; largest direction
plus weight arrays8388608B, process peak97267712B. Those are bounded offline
verification costs, not a proposed per-frame integration method.

Set `TMPDIR=/tmp`, one BLAS/OMP thread, `PYTHONDONTWRITEBYTECODE=1` and
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render`.
Run `leaf_gloss_energy.py --output /tmp/<fresh-energy-directory>` and
`leaf_gloss_render.py --inputs <prepared-six-leaf-source> --output /tmp/<fresh-directory>
--mode area` (or `additive`; `--mr-fraction 0` restores the old image).
Run `python -m unittest leaf_gloss_checks` for the independent controls.
The affected source and installed-library suites each pass117 tests. Ruff,
format check and docs lint pass. Private inputs, reports, PNGs and logs remain
under `/tmp/outshine-vegetation-leaf-gloss-proof-81510acfd`; duplicate zero/installed
exports and the copied Ghidra project were removed after exact verification.
No shared ABI/default changes: wax-layer semantics require Material/Light
agreement before adoption; filtering glints, volume transport, moving plants,
wind, mature architecture, stand LOD and worldwide recipes remain open.
