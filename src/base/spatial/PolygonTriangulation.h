#ifndef OUTSHINE_BASE_SPATIAL_POLYGONTRIANGULATION_H
#define OUTSHINE_BASE_SPATIAL_POLYGONTRIANGULATION_H

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine {

using PolygonRing = std::vector<std::array<double, 2>>;
enum class PolygonTriangulationError { InvalidPolygon, CapacityExceeded, LibraryFailure };

[[nodiscard]] std::expected<std::vector<uint32_t>, PolygonTriangulationError>
TriangulatePolygon(std::span<const PolygonRing> rings);

}
#endif
