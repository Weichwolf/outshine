#include "MaterialAsset.h"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {

void MaterialAssetSet::Adopt(std::vector<Core::Raster> &&images,
                             std::vector<MaterialAsset> &&materials) {
  Images_ = std::move(images);
  Materials_ = std::move(materials);
}

std::expected<void, std::string> MaterialAssetSet::CopyTo(Geometry &geometry) const {
  for (const Core::Raster &image : Images_) {
    const auto added = geometry.addImage(image.Width, image.Height, image.Rgba);
    if (!added) { return std::unexpected(std::string(Describe(added.error()))); }
  }
  for (const MaterialAsset &material : Materials_) {
    const auto added = geometry.addSurface(material.Name, material.Surface);
    if (!added) { return std::unexpected(std::string(Describe(added.error()))); }
  }
  return {};
}

std::string_view MaterialAssetSet::ErrorAt(int material) const noexcept {
  if (material < 0 || static_cast<size_t>(material) >= Materials_.size()) { return {}; }
  return Materials_[static_cast<size_t>(material)].Error;
}

}
