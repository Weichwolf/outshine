#ifndef OUTSHINE_RENDER_SURFACEBINDINGS_H
#define OUTSHINE_RENDER_SURFACEBINDINGS_H

#include <array>
#include "DrawList.h"
#include "KernelShape.h"
#include "SubjectTypes.h"
#include "scene/SurfaceState.h"

namespace outshine::Render {

struct SurfaceBindings {
  DrawShape Shape{.VertexUniformBuffers = 1, .VertexStorageBuffers = 1};
  std::array<uint32_t, 8> Images{};
  uint32_t Count = 0;

  constexpr SurfaceBindings(VertexLayout layout,
                            SurfaceKind kind,
                            SurfaceDomain domain,
                            long identityIndex) {
    const bool flat = !CarriesNormal(layout);
    const bool transmits = kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
    if (domain == SurfaceDomain::Ground) {
      Images[Count++] = 7;
      Shape.FragmentStorageBuffers = 3;
    } else if (flat) {
      if (transmits) {
        Images[Count++] = 6;
      } else if (CarriesUv(layout)) {
        Images[Count++] = 0;
      }
    } else {
      Shape.FragmentStorageBuffers = 1;
      if (CarriesUv(layout)) {
        for (uint32_t image = 0; image < 6; ++image) {
          if (image != 1 || CarriesTangent(layout)) { Images[Count++] = image; }
        }
      }
      if (transmits) { Images[Count++] = 6; }
      Images[Count++] = 7;
    }
    Shape.FragmentSamplers = Count;
    Shape.FragmentUniformBuffers =
        flat ? (Count > 0 || kind != SurfaceKind::Opaque || identityIndex >= 0 ? 1u : 0u) : 2u;
  }
};

} // namespace outshine::Render
#endif
