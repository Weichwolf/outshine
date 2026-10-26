#ifndef OUTSHINE_WORLD_PRODUCTS_GEOMETRYPRODUCTCODEC_H
#define OUTSHINE_WORLD_PRODUCTS_GEOMETRYPRODUCTCODEC_H

#include "Geometry.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeGeometryProduct(const Geometry &geometry,
                                                                        size_t bytesMost);

[[nodiscard]] std::optional<Geometry> DecodeGeometryProduct(std::span<const uint8_t> bytes,
                                                            size_t bytesMost);

}
#endif
