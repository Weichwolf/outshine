#ifndef MATERIAL_H
#define MATERIAL_H

namespace outshine {

/* HOW A SURFACE'S ALPHA IS READ, and it is a three-valued property of the material rather than a
 * magnitude anything infers. glTF 2.0 (`Specification.adoc:2178`): under `OPAQUE` "the rendered
 * output is fully opaque and any alpha value is ignored", under `MASK` it is cut at `alphaCutoff`,
 * under `BLEND` it is composited. A float cannot carry three answers (`Enum.2`), and deriving the
 * mode from `BaseColour[3] < 1` -- which is what this engine did -- turns an opaque material whose
 * texture happens to carry alpha into a masked one. */
enum class AlphaMode { Opaque, Masked, Blended };

/* A SURFACE IN glTF 2.0's METAL-ROUGH PARAMETERISATION, which is the one the Khronos corpus states
 * its criteria in, so the engine speaks it rather than translating into a second vocabulary.
 *
 * `BaseColour` IS NOT AN ALBEDO UNTIL `Metalness` SAYS SO. At metalness 0 its RGB is the diffuse
 * reflectance of a dielectric; at metalness 1 it is the conductor's normal-incidence reflectance.
 * The old field was named `Albedo` and carried no metalness at all, so a metal had no spelling and
 * the name was true of only half the range it covered.
 *
 * The defaults are one declared surface -- a fully rough, fully covering, opaque, non-transmitting
 * dielectric mid grey. [SET] 0.5 linear base colour, the neutral card; everything else is the
 * absence of an effect, so a row that declares nothing draws as matte grey rather than as black or
 * as glass. */
struct Material {
  /* Linear RGB plus alpha, glTF's `baseColorFactor` order. The alpha is the QUANTITY; what is done
   * with it is `Alpha`'s answer and never this field's. */
  float BaseColour[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  float Metalness = 0.0f;               /* 0 dielectric, 1 conductor; glTF's default is 1 and ours
                                         * is 0 because a surface nobody described is not a metal */
  float Roughness = 1.0f;               /* GGX perceptual roughness, 0..1 */
  float Transmission = 0.0f;            /* fraction passed through the sheet, 0..1 */
  float Ior = 1.5f;                     /* refractive index; glTF's dielectric default, F0 = 0.04 */
  float Emission[3] = {0.0f, 0.0f, 0.0f}; /* cd/m^2 */
  AlphaMode Alpha = AlphaMode::Opaque;
  /* glTF's `doubleSided`, and it lives here rather than beside the reader because it is a statement
   * about the SURFACE that only the pipeline can honour: a single-sided facet is culled from behind
   * and a double-sided one is not. `TextureSettingsTest` puts a polygon facing the wrong way in
   * front of a test polygon and lets the flag decide which of the two is seen, so an engine that
   * carries the flag no further than its reader draws the wrong cell. glTF's own default is false. */
  bool DoubleSided = false;
  /* Below this alpha a `Masked` fragment is discarded. 0.5 is the format's own default and not an
   * argument made here (`Specification.adoc`, `alphaCutoff`); `AlphaBlendModeTest` renders 0.25 and
   * 0.75 columns that fail an engine carrying one number for the whole scene. */
  float CoverageCut = 0.5f;
  /* `KHR_materials_unlit`: the base colour IS the radiance the surface leaves, so no light, no
   * normal and no BRDF enter it. It is a property of the SURFACE and not a quality setting, which is
   * why it sits beside the other surface answers rather than beside a renderer's options -- and it
   * moves no pipeline state, because what changes is which radiance a part declares and not how the
   * fragment is composited. */
  bool Unlit = false;
};

/* A MATERIAL AS THE PICTURE TAKES IT: a row of numbers, and nothing in it can switch a pipeline
 * state (the deleted architecture document). Its field meanings live in the shader that reads the row and are
 * written down nowhere else, which is what keeps a content taxonomy out of the engine. */
constexpr int kMaterialRowFloats = 20;

} // namespace outshine
#endif
