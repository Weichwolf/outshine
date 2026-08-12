#ifndef SURFACESTATE_H
#define SURFACESTATE_H

#include "Material.h"

namespace outshine {

enum class SurfaceKind { Opaque, Masked, Blended, ThinTransmissive, Refractive };

/* WHETHER A MESH'S TRIANGLE ORDER CAN BE TRUSTED — the half of "facing" a material cannot state.
 * An OSM ring arrives wound either way, so a prism extruded from one has no reliable outward side;
 * a mesh a generator grew does. Culling needs both answers and neither substitutes for the other. */
enum class Winding { Trusted, Unknown };

class SurfaceState;
constexpr SurfaceState StateOf(const Material &material);

class SurfaceState {
public:
  [[nodiscard]] constexpr SurfaceKind Kind() const { return Kind_; }
  [[nodiscard]] constexpr bool WritesDepth() const { return WritesDepth_; }
  [[nodiscard]] constexpr bool CullsBack() const { return CullsBack_; }
  [[nodiscard]] constexpr bool Blends() const { return Blends_; }
  [[nodiscard]] constexpr bool Emits() const { return Emits_; }
  /* Below this alpha a `Masked` fragment is discarded. The number is the MATERIAL's -- the format
   * carries one per material and `AlphaBlendModeTest` renders three different ones in one file --
   * so this only carries it through and states no value of its own. */
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
  s.CoverageCut_ = material.CoverageCut;
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
  switch (material.Alpha) {
    case AlphaMode::Opaque: break;
    case AlphaMode::Masked: s.Kind_ = SurfaceKind::Masked; break;
    /* A blended surface is composited in the order it is drawn, so it cannot also write the depth
     * that would hide what is behind it -- the same pair the refractive arm above states. */
    case AlphaMode::Blended:
      s.Kind_ = SurfaceKind::Blended;
      s.WritesDepth_ = false;
      s.Blends_ = true;
      break;
  }
  return s;
}

[[nodiscard]] constexpr bool CullsBackFaces(const SurfaceState &state, Winding winding) {
  return state.CullsBack() && winding == Winding::Trusted;
}

} // namespace outshine
#endif
