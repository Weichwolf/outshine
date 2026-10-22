#ifndef OUTSHINE_RENDER_FIELD_FIELDSURFACE_H
#define OUTSHINE_RENDER_FIELD_FIELDSURFACE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
enum class FieldFace : uint32_t { XWall, ZWall, Roof, Floor };

struct alignas(16) FieldMaskGpu {
  std::array<float, 4> OriginPitch{};
  std::array<uint32_t, 4> Shape{};
  std::array<float, 4> Coverage{};
};

struct alignas(16) FieldVertex {
  std::array<float, 3> Position{};
  FieldFace Kind = FieldFace::Floor;
  std::array<float, 3> Normal{};
  uint32_t FixedCell = 0;
  std::array<uint32_t, 2> PatchCell{};
  std::array<float, 2> LevelReserved{};
};

static_assert(sizeof(FieldMaskGpu) == 48 && alignof(FieldMaskGpu) == 16);
static_assert(offsetof(FieldMaskGpu, OriginPitch) == 0 && offsetof(FieldMaskGpu, Shape) == 16 &&
              offsetof(FieldMaskGpu, Coverage) == 32);
static_assert(sizeof(FieldFace) == sizeof(uint32_t));
static_assert(sizeof(FieldVertex) == 48 && alignof(FieldVertex) == 16);
static_assert(offsetof(FieldVertex, Position) == 0 && offsetof(FieldVertex, Kind) == 12 &&
              offsetof(FieldVertex, Normal) == 16 && offsetof(FieldVertex, FixedCell) == 28 &&
              offsetof(FieldVertex, PatchCell) == 32 && offsetof(FieldVertex, LevelReserved) == 40);

struct FieldSurface {
  FieldMaskGpu Parameters;
  std::vector<FieldVertex> Vertices;
  std::vector<uint32_t> Indices;
};

enum class FieldSurfaceError { InvalidShape, InvalidMetric, InvalidHeight };

[[nodiscard]] std::expected<FieldSurface, FieldSurfaceError>
BuildFieldSurface(const FieldMaskGpu &parameters, std::span<const float> heights);
}
#endif
