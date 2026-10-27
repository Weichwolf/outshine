#ifndef OUTSHINE_ENGINE_MATERIALBINDINGS_H
#define OUTSHINE_ENGINE_MATERIALBINDINGS_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "RenderGeometryView.h"
#include "MaterialResolution.h"
#include "Geometry.h"
#include "Material.h"

namespace outshine::Core {

struct MaterialOverride {
  std::string MaterialName;
  std::string PartName;
  int PartIndex = -1;
  bool RetainMaps = false;
  Material Surface;
};

class MaterialBindings {
public:
  void Clear();

  [[nodiscard]] std::expected<void, std::string>
  Resolve(const Geometry &native,
          const Render::RenderGeometryView &geometryView,
          const Material &fallback,
          std::span<const MaterialOverride> overrides,
          int groundSurface,
          std::string_view objectName);

  [[nodiscard]] std::span<const Render::MaterialBinding> Slots() const noexcept {
    return Table_.Slots;
  }

  [[nodiscard]] std::span<const uint32_t> PartSlots() const noexcept { return Table_.PartSlot; }

  [[nodiscard]] std::vector<uint32_t> NativeSurfaceSlots(size_t surfaceCount) const;

private:
  Render::SurfaceTable Table_;
};

}
#endif
