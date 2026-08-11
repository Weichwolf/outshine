#ifndef MATERIAL_H
#define MATERIAL_H

namespace outshine {

/* The defaults are one declared surface — a fully rough, fully covering, non-transmitting mid grey.
 * [SET] 0.5 linear albedo, the neutral card; everything else is the absence of an effect, so a row
 * that declares nothing draws as matte grey rather than as black or as glass. */
struct Material {
  float Albedo[3] = {0.5f, 0.5f, 0.5f}; /* linear reflectance, 0..1 */
  float Roughness = 1.0f;               /* GGX perceptual roughness, 0..1 */
  float Coverage = 1.0f;                /* fraction of the primitive the surface fills, 0..1 */
  float Transmission = 0.0f;            /* fraction passed through the sheet, 0..1 */
  float Ior = 1.0f;                     /* refractive index; 1 = no refraction */
  float Emission[3] = {0.0f, 0.0f, 0.0f}; /* cd/m^2 */
};

/* A MATERIAL AS THE PICTURE TAKES IT: a row of numbers, and nothing in it can switch a pipeline
 * state (doc/architecture.md). Its field meanings live in the shader that reads the row and are
 * written down nowhere else, which is what keeps a content taxonomy out of the engine. */
constexpr int kMaterialRowFloats = 20;

} // namespace outshine
#endif
