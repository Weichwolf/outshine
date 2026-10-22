#include "FieldSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kMaximumSide = 256;
constexpr uint32_t kVerticesPerQuad = 4;
constexpr std::array<uint32_t, 6> kQuadIndices{0, 1, 2, 0, 2, 3};

struct Patch {
  uint32_t Column = 0, Row = 0, Columns = 0, Rows = 0;
  float HighestM = 0;
  std::vector<float> Levels;
};

bool Valid(const FieldMaskGpu &p, std::span<const float> heights) {
  const auto &s = p.Shape;
  return s[0] > 0 && s[1] > 0 && s[0] <= kMaximumSide && s[1] <= kMaximumSide && s[2] > 0 &&
         s[2] <= kMaximumSide && s[3] == 0 && heights.size() == static_cast<size_t>(s[0]) * s[1];
}

bool Metric(const FieldMaskGpu &p) {
  return std::ranges::all_of(p.OriginPitch, [](float v) { return std::isfinite(v); }) &&
         p.OriginPitch[3] > 0 && std::isfinite(p.Coverage[0]) && p.Coverage[0] > 0 &&
         p.Coverage[0] <= 1 && p.Coverage[1] == 0 && p.Coverage[2] == 0 && p.Coverage[3] == 0;
}

void Quad(FieldSurface &out,
          const Patch &patch,
          FieldFace kind,
          uint32_t fixed,
          float level,
          const std::array<float, 3> &centre,
          const std::array<float, 3> &across,
          const std::array<float, 3> &up,
          const std::array<float, 3> &normal) {
  constexpr std::array<std::array<float, 2>, kVerticesPerQuad> corners{
      {{{-1, -1}}, {{1, -1}}, {{1, 1}}, {{-1, 1}}}};
  const auto first = static_cast<uint32_t>(out.Vertices.size());
  for (const auto &corner : corners) {
    FieldVertex vertex;
    vertex.Kind = kind;
    vertex.FixedCell = fixed;
    vertex.PatchCell = {patch.Column, patch.Row};
    vertex.Normal = normal;
    vertex.LevelReserved = {level, 0};
    for (size_t axis = 0; axis < vertex.Position.size(); ++axis) {
      vertex.Position[axis] = centre[axis] + across[axis] * corner[0] + up[axis] * corner[1];
    }
    out.Vertices.push_back(vertex);
  }
  for (const auto index : kQuadIndices) { out.Indices.push_back(first + index); }
}

Patch ReadPatch(const FieldMaskGpu &p,
                std::span<const float> heights,
                uint32_t column,
                uint32_t row) {
  Patch patch{.Column = column,
              .Row = row,
              .Columns = std::min(p.Shape[2], p.Shape[0] - column),
              .Rows = std::min(p.Shape[2], p.Shape[1] - row),
              .Levels = {}};
  patch.Levels.reserve(static_cast<size_t>(patch.Columns) * patch.Rows);
  for (uint32_t z = 0; z < patch.Rows; ++z) {
    for (uint32_t x = 0; x < patch.Columns; ++x) {
      const float height = heights[static_cast<size_t>(row + z) * p.Shape[0] + column + x];
      if (height > 0) { patch.Levels.push_back(height); }
    }
  }
  std::ranges::sort(patch.Levels);
  const auto repeated = std::ranges::unique(patch.Levels);
  patch.Levels.erase(repeated.begin(), repeated.end());
  if (!patch.Levels.empty()) { patch.HighestM = patch.Levels.back(); }
  return patch;
}

void Walls(FieldSurface &out, const Patch &patch) {
  const auto &p = out.Parameters;
  const float pitch = p.OriginPitch[3];
  const float inset = (1 - p.Coverage[0]) * .5f;
  const float x =
      p.OriginPitch[0] + (static_cast<float>(patch.Column) + .5f * patch.Columns) * pitch;
  const float z = p.OriginPitch[2] + (static_cast<float>(patch.Row) + .5f * patch.Rows) * pitch;
  const float y = p.OriginPitch[1] + .5f * patch.HighestM;
  const float halfHeight = .5f * patch.HighestM;
  for (uint32_t column = 0; column < patch.Columns; ++column) {
    const float start = p.OriginPitch[0] + static_cast<float>(patch.Column + column) * pitch;
    Quad(out,
         patch,
         FieldFace::XWall,
         column,
         0,
         {start + inset * pitch, y, z},
         {0, 0, .5f * patch.Rows * pitch},
         {0, halfHeight, 0},
         {-1, 0, 0});
    Quad(out,
         patch,
         FieldFace::XWall,
         column,
         0,
         {start + (1 - inset) * pitch, y, z},
         {0, 0, -.5f * patch.Rows * pitch},
         {0, halfHeight, 0},
         {1, 0, 0});
  }
  for (uint32_t row = 0; row < patch.Rows; ++row) {
    const float start = p.OriginPitch[2] + static_cast<float>(patch.Row + row) * pitch;
    Quad(out,
         patch,
         FieldFace::ZWall,
         row,
         0,
         {x, y, start + inset * pitch},
         {-.5f * patch.Columns * pitch, 0, 0},
         {0, halfHeight, 0},
         {0, 0, -1});
    Quad(out,
         patch,
         FieldFace::ZWall,
         row,
         0,
         {x, y, start + (1 - inset) * pitch},
         {.5f * patch.Columns * pitch, 0, 0},
         {0, halfHeight, 0},
         {0, 0, 1});
  }
}

void Horizontal(FieldSurface &out, const Patch &patch) {
  const auto &p = out.Parameters;
  const float width = static_cast<float>(patch.Columns) * p.OriginPitch[3];
  const float depth = static_cast<float>(patch.Rows) * p.OriginPitch[3];
  const float x =
      p.OriginPitch[0] + static_cast<float>(patch.Column) * p.OriginPitch[3] + .5f * width;
  const float z = p.OriginPitch[2] + static_cast<float>(patch.Row) * p.OriginPitch[3] + .5f * depth;
  for (const float level : patch.Levels) {
    Quad(out,
         patch,
         FieldFace::Roof,
         0,
         level,
         {x, p.OriginPitch[1] + level, z},
         {.5f * width, 0, 0},
         {0, 0, -.5f * depth},
         {0, 1, 0});
  }
  Quad(out,
       patch,
       FieldFace::Floor,
       0,
       0,
       {x, p.OriginPitch[1], z},
       {.5f * width, 0, 0},
       {0, 0, .5f * depth},
       {0, -1, 0});
}
}

std::expected<FieldSurface, FieldSurfaceError> BuildFieldSurface(const FieldMaskGpu &parameters,
                                                                 std::span<const float> heights) {
  if (!Valid(parameters, heights)) { return std::unexpected(FieldSurfaceError::InvalidShape); }
  if (!Metric(parameters)) { return std::unexpected(FieldSurfaceError::InvalidMetric); }
  if (!std::ranges::all_of(heights, [](float h) { return std::isfinite(h) && h >= 0; })) {
    return std::unexpected(FieldSurfaceError::InvalidHeight);
  }
  FieldSurface out{.Parameters = parameters, .Vertices = {}, .Indices = {}};
  for (uint32_t row = 0; row < parameters.Shape[1]; row += parameters.Shape[2]) {
    for (uint32_t column = 0; column < parameters.Shape[0]; column += parameters.Shape[2]) {
      const auto patch = ReadPatch(parameters, heights, column, row);
      if (patch.HighestM == 0) { continue; }
      Walls(out, patch);
      Horizontal(out, patch);
    }
  }
  if (!std::ranges::all_of(out.Vertices, [](const FieldVertex &v) {
        return std::ranges::all_of(v.Position, [](float p) { return std::isfinite(p); });
      })) {
    return std::unexpected(FieldSurfaceError::InvalidMetric);
  }
  return out;
}
}
