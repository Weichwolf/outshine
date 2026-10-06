#ifndef OUTSHINE_RENDER_STAGES_SURFACEBINDINGS_H
#define OUTSHINE_RENDER_STAGES_SURFACEBINDINGS_H

#include <array>
#include "DrawList.h"
#include "KernelShape.h"
#include "SubjectTypes.h"
#include "SurfaceState.h"

namespace outshine::Render {

struct SurfaceBindings {
  DrawShape Shape{.VertexUniformBuffers = 1, .VertexStorageBuffers = 1};
  std::array<uint32_t, kSubjectImages> Images{};
  uint32_t Count = 0;

  constexpr SurfaceBindings(VertexLayout layout,
                            SurfaceKind kind,
                            SurfaceDomain domain,
                            long identityIndex,
                            bool capture = false) {
    Shape.VertexStorageBuffers =
        1u + (CarriesColour(layout) ? 1u : 0u) + (CarriesTangent(layout) ? 1u : 0u);
    const bool transmits = kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
    if (domain == SurfaceDomain::Ground) {
      AddLightingImages(false, capture);
      Shape.FragmentStorageBuffers = capture ? 2u : 3u;
      Shape.FragmentUniformBuffers = 2;
    } else if (!CarriesNormal(layout)) {
      FlatResources(layout, kind, identityIndex, transmits);
    } else {
      LitResources(layout, transmits, capture);
    }
    Shape.FragmentSamplers = Count;
  }

private:
  constexpr void AddLightingImages(bool transmits, bool capture) {
    if (capture) { return; }
    if (transmits) { Images[Count++] = 6; }
    Images[Count++] = 7;
    Images[Count++] = 8;
  }

  constexpr void
  FlatResources(VertexLayout layout, SurfaceKind kind, long identityIndex, bool transmits) {
    if (transmits) {
      Images[Count++] = 6;
    } else if (CarriesUv(layout)) {
      Images[Count++] = 0;
    }
    Shape.FragmentUniformBuffers =
        Count > 0 || kind != SurfaceKind::Opaque || identityIndex >= 0 ? 1u : 0u;
  }

  constexpr void LitResources(VertexLayout layout, bool transmits, bool capture) {
    Shape.FragmentStorageBuffers = capture ? 0u : 1u;
    Shape.FragmentUniformBuffers = capture ? 1u : 2u;
    if (CarriesUv(layout)) {
      const uint32_t images = capture ? 3u : 6u;
      for (uint32_t image = 0; image < images; ++image) {
        if (image != 1 || CarriesTangent(layout)) { Images[Count++] = image; }
      }
    }
    AddLightingImages(transmits, capture);
  }
};

}
#endif
