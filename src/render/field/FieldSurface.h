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

inline constexpr size_t kFieldMaskBytes = 48;
inline constexpr size_t kFieldVertexBytes = 48;
inline constexpr size_t kFieldFixedCellOffset = 28;
inline constexpr size_t kFieldLevelOffset = 40;

static_assert(sizeof(FieldMaskGpu) == kFieldMaskBytes && alignof(FieldMaskGpu) == 16);
static_assert(offsetof(FieldMaskGpu, OriginPitch) == 0 && offsetof(FieldMaskGpu, Shape) == 16 &&
              offsetof(FieldMaskGpu, Coverage) == 32);
static_assert(sizeof(FieldFace) == sizeof(uint32_t));
static_assert(sizeof(FieldVertex) == kFieldVertexBytes && alignof(FieldVertex) == 16);
static_assert(offsetof(FieldVertex, Position) == 0 && offsetof(FieldVertex, Kind) == 12 &&
              offsetof(FieldVertex, Normal) == 16 &&
              offsetof(FieldVertex, FixedCell) == kFieldFixedCellOffset &&
              offsetof(FieldVertex, PatchCell) == 32 &&
              offsetof(FieldVertex, LevelReserved) == kFieldLevelOffset);

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
