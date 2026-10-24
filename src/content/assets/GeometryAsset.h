#ifndef OUTSHINE_CONTENT_GEOMETRYASSET_H
#define OUTSHINE_CONTENT_GEOMETRYASSET_H

#include "Geometry.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeGeometryAsset(const Geometry &geometry,
                                                                      size_t bytesMost);

[[nodiscard]] std::optional<Geometry> DecodeGeometryAsset(std::span<const uint8_t> bytes,
                                                          size_t bytesMost);

}
#endif
