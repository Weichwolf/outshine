#ifndef OUTSHINE_CONTENT_SHADE_MATERIALASSET_H
#define OUTSHINE_CONTENT_SHADE_MATERIALASSET_H

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "Image.h"
#include "scene/Geometry.h"

namespace outshine {

struct MaterialAsset {
  std::string Name;
  Material Surface;
  std::string Error;
};

class MaterialAssetSet {
public:
  void Adopt(std::vector<Core::Raster> &&images, std::vector<MaterialAsset> &&materials);
  [[nodiscard]] std::expected<void, std::string> CopyTo(Geometry &geometry) const;
  [[nodiscard]] std::string_view ErrorAt(int material) const noexcept;

  [[nodiscard]] size_t MaterialCount() const noexcept { return Materials_.size(); }

private:
  std::vector<Core::Raster> Images_;
  std::vector<MaterialAsset> Materials_;
};

}

#endif
