#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#include "AssetCache.h"
#include "Geometry.h"
#include "Mat4.h"
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace outshine {
class GeometryAsset {
public:
  struct Description {
    Box Bounds;
    Mat4 WorldFromLocal;
  };

  struct Product {
    Description Layout;
    Geometry Mesh;
  };

  [[nodiscard]] static std::expected<std::optional<Description>, AssetCacheError>
  Describe(const AssetCache &cache, std::string_view key, std::string_view parent);
  [[nodiscard]] static std::expected<std::optional<Product>, AssetCacheError>
  Load(const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost);
  // bytesMost bounds the encoded mesh, not codec workspace or total producer residency.
  [[nodiscard]] static std::expected<void, AssetCacheError> Publish(AssetCache &cache,
                                                                    const std::string &key,
                                                                    const std::string &parent,
                                                                    const Description &description,
                                                                    const Geometry &geometry,
                                                                    size_t bytesMost);
};
}
#endif
