#include "FieldSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kMaximumSide = 256;
constexpr uint32_t kVerticesPerQuad = 4;
constexpr std::array<uint32_t, 6> kQuadIndices{0, 1, 2, 0, 2, 3};

struct Roof {
  float Height = 0;
  uint32_t Left = 0, Top = 0, Right = 0, Bottom = 0;
};

struct Patch {
  uint32_t Column = 0, Row = 0, Columns = 0, Rows = 0;
  float HighestM = 0;
  std::vector<Roof> Levels;
  std::vector<float> ColumnTops, RowTops;
};

struct FacePlane {
  FieldFace Kind = FieldFace::Floor;
  uint32_t FixedCell = 0;
  float Level = 0;
  std::array<float, 3> Centre{}, Across{}, Up{}, Normal{};
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

void Quad(FieldSurface &out, const Patch &patch, const FacePlane &plane) {
  constexpr std::array<std::array<float, 2>, kVerticesPerQuad> corners{
      {{{-1, -1}}, {{1, -1}}, {{1, 1}}, {{-1, 1}}}};
  const auto first = static_cast<uint32_t>(out.Vertices.size());
  for (const auto &corner : corners) {
    FieldVertex vertex;
    vertex.Kind = plane.Kind;
    vertex.FixedCell = plane.FixedCell;
    vertex.PatchCell = {patch.Column, patch.Row};
    vertex.Normal = plane.Normal;
    vertex.LevelReserved = {plane.Level, 0};
    for (size_t axis = 0; axis < vertex.Position.size(); ++axis) {
      vertex.Position[axis] =
          plane.Centre[axis] + plane.Across[axis] * corner[0] + plane.Up[axis] * corner[1];
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
              .Levels = {},
              .ColumnTops = {},
              .RowTops = {}};
  patch.ColumnTops.resize(patch.Columns);
  patch.RowTops.resize(patch.Rows);
  patch.Levels.reserve(static_cast<size_t>(patch.Columns) * patch.Rows);
  for (uint32_t z = 0; z < patch.Rows; ++z) {
    for (uint32_t x = 0; x < patch.Columns; ++x) {
      const float height = heights[static_cast<size_t>(row + z) * p.Shape[0] + column + x];
      if (height > 0) {
        patch.Levels.push_back(
            {.Height = height, .Left = x, .Top = z, .Right = x + 1, .Bottom = z + 1});
        patch.ColumnTops[x] = std::max(patch.ColumnTops[x], height);
        patch.RowTops[z] = std::max(patch.RowTops[z], height);
      }
    }
  }
  std::ranges::sort(patch.Levels, {}, &Roof::Height);
  size_t kept = 0;
  for (size_t at = 0; at < patch.Levels.size(); ++at) {
    const auto cell = patch.Levels[at];
    if (kept == 0 || patch.Levels[kept - 1].Height != cell.Height) {
      patch.Levels[kept++] = cell;
    } else {
      auto &roof = patch.Levels[kept - 1];
      roof.Left = std::min(roof.Left, cell.Left);
      roof.Top = std::min(roof.Top, cell.Top);
      roof.Right = std::max(roof.Right, cell.Right);
      roof.Bottom = std::max(roof.Bottom, cell.Bottom);
    }
  }
  patch.Levels.resize(kept);
  if (!patch.Levels.empty()) { patch.HighestM = patch.Levels.back().Height; }
  return patch;
}

void Walls(FieldSurface &out, const Patch &patch) {
  const auto &p = out.Parameters;
  const float pitch = p.OriginPitch[3];
  const float inset = (1 - p.Coverage[0]) * .5f;
  const auto columns = static_cast<float>(patch.Columns);
  const auto rows = static_cast<float>(patch.Rows);
  const float x = p.OriginPitch[0] + (static_cast<float>(patch.Column) + .5f * columns) * pitch;
  const float z = p.OriginPitch[2] + (static_cast<float>(patch.Row) + .5f * rows) * pitch;
  for (uint32_t column = 0; column < patch.Columns; ++column) {
    const float halfHeight = .5f * patch.ColumnTops[column];
    if (halfHeight == 0) { continue; }
    const float y = p.OriginPitch[1] + halfHeight;
    const float start = p.OriginPitch[0] + static_cast<float>(patch.Column + column) * pitch;
    Quad(out,
         patch,
         {.Kind = FieldFace::XWall,
          .FixedCell = column,
          .Centre = {start + inset * pitch, y, z},
          .Across = {0, 0, .5f * rows * pitch},
          .Up = {0, halfHeight, 0},
          .Normal = {-1, 0, 0}});
    Quad(out,
         patch,
         {.Kind = FieldFace::XWall,
          .FixedCell = column,
          .Centre = {start + (1 - inset) * pitch, y, z},
          .Across = {0, 0, -.5f * rows * pitch},
          .Up = {0, halfHeight, 0},
          .Normal = {1, 0, 0}});
  }
  for (uint32_t row = 0; row < patch.Rows; ++row) {
    const float halfHeight = .5f * patch.RowTops[row];
    if (halfHeight == 0) { continue; }
    const float y = p.OriginPitch[1] + halfHeight;
    const float start = p.OriginPitch[2] + static_cast<float>(patch.Row + row) * pitch;
    Quad(out,
         patch,
         {.Kind = FieldFace::ZWall,
          .FixedCell = row,
          .Centre = {x, y, start + inset * pitch},
          .Across = {-.5f * columns * pitch, 0, 0},
          .Up = {0, halfHeight, 0},
          .Normal = {0, 0, -1}});
    Quad(out,
         patch,
         {.Kind = FieldFace::ZWall,
          .FixedCell = row,
          .Centre = {x, y, start + (1 - inset) * pitch},
          .Across = {.5f * columns * pitch, 0, 0},
          .Up = {0, halfHeight, 0},
          .Normal = {0, 0, 1}});
  }
}

void Horizontal(FieldSurface &out, const Patch &patch) {
  const auto &p = out.Parameters;
  const float width = static_cast<float>(patch.Columns) * p.OriginPitch[3];
  const float depth = static_cast<float>(patch.Rows) * p.OriginPitch[3];
  const float x =
      p.OriginPitch[0] + static_cast<float>(patch.Column) * p.OriginPitch[3] + .5f * width;
  const float z = p.OriginPitch[2] + static_cast<float>(patch.Row) * p.OriginPitch[3] + .5f * depth;
  const float inset = .5f * (1 - p.Coverage[0]);
  for (const auto &level : patch.Levels) {
    const float left = static_cast<float>(patch.Column + level.Left) + inset;
    const float top = static_cast<float>(patch.Row + level.Top) + inset;
    const float roofWidth =
        (static_cast<float>(level.Right - level.Left) - 2 * inset) * p.OriginPitch[3];
    const float roofDepth =
        (static_cast<float>(level.Bottom - level.Top) - 2 * inset) * p.OriginPitch[3];
    Quad(out,
         patch,
         {.Kind = FieldFace::Roof,
          .Level = level.Height,
          .Centre = {p.OriginPitch[0] + left * p.OriginPitch[3] + .5f * roofWidth,
                     p.OriginPitch[1] + level.Height,
                     p.OriginPitch[2] + top * p.OriginPitch[3] + .5f * roofDepth},
          .Across = {.5f * roofWidth, 0, 0},
          .Up = {0, 0, -.5f * roofDepth},
          .Normal = {0, 1, 0}});
  }
  Quad(out,
       patch,
       {.Kind = FieldFace::Floor,
        .Centre = {x, p.OriginPitch[1], z},
        .Across = {.5f * width, 0, 0},
        .Up = {0, 0, .5f * depth},
        .Normal = {0, -1, 0}});
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
