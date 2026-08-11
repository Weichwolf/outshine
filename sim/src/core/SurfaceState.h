#ifndef SURFACESTATE_H
#define SURFACESTATE_H

#include "Material.h"

namespace outshine {

enum class SurfaceKind { Opaque, Masked, ThinTransmissive, Refractive };

/* WHETHER A MESH'S TRIANGLE ORDER CAN BE TRUSTED — the half of "facing" a material cannot state.
 * An OSM ring arrives wound either way, so a prism extruded from one has no reliable outward side;
 * a mesh a generator grew does. Culling needs both answers and neither substitutes for the other. */
enum class Winding { Trusted, Unknown };

class SurfaceState;
constexpr SurfaceState StateOf(const Material &material);

class SurfaceState {
public:
  constexpr SurfaceKind Kind() const { return Kind_; }
  constexpr bool WritesDepth() const { return WritesDepth_; }
  constexpr bool CullsBack() const { return CullsBack_; }
  constexpr bool Blends() const { return Blends_; }
  constexpr bool Emits() const { return Emits_; }
  /* Below this coverage the fragment is discarded; above it the surface is opaque. [SET] half:
   * with one cut and no dithering it is the value that neither thins nor fattens a leaf edge, and
   * what replaces it is a measured coverage-to-area curve per material. */
  constexpr float CoverageCut() const { return CoverageCut_; }

private:
  constexpr SurfaceState() = default;
  friend constexpr SurfaceState StateOf(const Material &material);

  SurfaceKind Kind_ = SurfaceKind::Opaque;
  bool WritesDepth_ = true;
  bool CullsBack_ = true;
  bool Blends_ = false;
  bool Emits_ = false;
  float CoverageCut_ = 0.5f;
};

constexpr SurfaceState StateOf(const Material &material) {
  SurfaceState s;
  s.Emits_ = material.Emission[0] > 0.0f || material.Emission[1] > 0.0f ||
             material.Emission[2] > 0.0f;
  if (material.Transmission > 0.0f && material.Ior > 1.0f) {
    s.Kind_ = SurfaceKind::Refractive;
    s.WritesDepth_ = false;
    s.Blends_ = true;
    return s;
  }
  if (material.Transmission > 0.0f) {
    s.Kind_ = SurfaceKind::ThinTransmissive;
    s.CullsBack_ = false;
    return s;
  }
  if (material.Coverage < 1.0f) s.Kind_ = SurfaceKind::Masked;
  return s;
}

constexpr bool CullsBackFaces(const SurfaceState &state, Winding winding) {
  return state.CullsBack() && winding == Winding::Trusted;
}

} // namespace outshine
#endif
