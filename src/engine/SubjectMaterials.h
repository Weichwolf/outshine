#ifndef OUTSHINE_ENGINE_SUBJECTMATERIALS_H
#define OUTSHINE_ENGINE_SUBJECTMATERIALS_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Shape.h"
#include "Surfacing.h"
#include "scene/Geometry.h"
#include "scene/Material.h"

namespace outshine::Core {

struct SubjectSurfaceOverride {
  std::string MaterialName;
  std::string PartName;
  int PartIndex = -1;
  bool RetainMaps = false;
  Material Surface;
};

class SubjectMaterials {
public:
  void Clear();

  [[nodiscard]] std::expected<void, std::string>
  Resolve(const Geometry &native,
          const Render::Shape &shaped,
          const Material &fallback,
          std::span<const SubjectSurfaceOverride> overrides,
          int groundSurface,
          std::string_view subjectName);

  [[nodiscard]] std::span<const Render::SubjectMaterial> Slots() const noexcept {
    return Table_.Slots;
  }

  [[nodiscard]] std::span<const uint32_t> PartSlots() const noexcept { return Table_.PartSlot; }

  [[nodiscard]] std::vector<uint32_t> NativeSurfaceSlots(size_t surfaceCount) const;

private:
  Render::SurfaceTable Table_;
};

}
#endif
