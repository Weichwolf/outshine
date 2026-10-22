#include "FieldSurface.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void CheckFaces(const FieldSurface &surface) {
  for (size_t first = 0; first < surface.Vertices.size(); first += 4) {
    const auto &a = surface.Vertices[first];
    const auto &b = surface.Vertices[first + 1];
    const auto &c = surface.Vertices[first + 2];
    std::array<float, 3> ab{}, ac{};
    for (size_t axis = 0; axis < 3; ++axis) {
      ab[axis] = b.Position[axis] - a.Position[axis];
      ac[axis] = c.Position[axis] - a.Position[axis];
    }
    const std::array cross{ab[1] * ac[2] - ab[2] * ac[1],
                           ab[2] * ac[0] - ab[0] * ac[2],
                           ab[0] * ac[1] - ab[1] * ac[0]};
    CHECK(cross[0] * a.Normal[0] + cross[1] * a.Normal[1] + cross[2] * a.Normal[2] > 0,
          "each nondegenerate face winds toward its outward normal");
    for (size_t corner = 0; corner < 4; ++corner) {
      const auto &v = surface.Vertices[first + corner];
      CHECK(v.Kind == a.Kind && v.FixedCell == a.FixedCell && v.PatchCell == a.PatchCell &&
                v.LevelReserved == a.LevelReserved && v.Normal == a.Normal,
            "both triangles carry exactly the same categorical and roof metadata");
      CHECK(v.Position[0] >= 10 && v.Position[0] <= 40 && v.Position[1] >= 3 &&
                v.Position[1] <= 10 && v.Position[2] >= -20 && v.Position[2] <= 0,
            "all patch faces remain inside the translated metric field bounds");
    }
  }
  for (const auto index : surface.Indices) {
    CHECK(index < surface.Vertices.size(), "every triangle references an existing vertex");
  }
}
}

int main() {
  FieldMaskGpu parameters{
      .OriginPitch = {10, 3, -20, 10}, .Shape = {3, 2, 2, 0}, .Coverage = {.75f, 0, 0, 0}};
  std::array heights{2.f, 0.f, 7.f, 2.f, 5.f, 7.f};
  const auto surface = BuildFieldSurface(parameters, heights);
  CHECK(surface && surface->Vertices.size() == 76 && surface->Indices.size() == 114,
        "full and partial patches emit 4N walls, distinct positive roofs and one floor");
  if (surface) {
    CheckFaces(*surface);
    std::array<uint32_t, 3> roofLevels{};
    float roofArea = 0;
    for (size_t first = 0; first < surface->Vertices.size(); first += 4) {
      const auto &v = surface->Vertices[first];
      if ((v.Kind == FieldFace::XWall || v.Kind == FieldFace::ZWall) &&
          v.PatchCell == std::array<uint32_t, 2>{0, 0} && v.FixedCell == 0) {
        for (size_t corner = 0; corner < 4; ++corner) {
          CHECK(surface->Vertices[first + corner].Position[1] <= 5,
                "wall envelopes stop at their occupied row or column height");
        }
      }
      if (v.Kind != FieldFace::Roof) { continue; }
      const auto &opposite = surface->Vertices[first + 2];
      roofArea +=
          std::abs((opposite.Position[0] - v.Position[0]) * (opposite.Position[2] - v.Position[2]));
      roofLevels[0] += v.LevelReserved[0] == heights[0];
      roofLevels[1] += v.LevelReserved[0] == heights[4];
      roofLevels[2] += v.LevelReserved[0] == heights[2];
      CHECK(v.Position[1] == parameters.OriginPitch[1] + v.LevelReserved[0],
            "roof geometry and exact stored height use the same translated origin");
    }
    CHECK((roofLevels == std::array<uint32_t, 3>{1, 1, 1}),
          "repeated roof heights produce one plane per distinct level and patch");
    CHECK(roofArea == 318.75f,
          "roof envelopes cover occupied level bounds instead of three complete patch planes");
  }
  heights.fill(0);
  const auto empty = BuildFieldSurface(parameters, heights);
  CHECK(empty && empty->Vertices.empty() && empty->Indices.empty(),
        "empty fields create no raster work");
  heights[0] = 1;
  const auto isolated = BuildFieldSurface(parameters, heights);
  CHECK(isolated && isolated->Vertices.size() == 24 && isolated->Indices.size() == 36,
        "unoccupied rows and columns emit no wall planes");
  heights[0] = 0;
  heights[1] = -1;
  CHECK(!BuildFieldSurface(parameters, heights), "negative height fails before geometry upload");
  heights[1] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!BuildFieldSurface(parameters, heights), "nonfinite height fails before geometry upload");
  heights.fill(1);
  parameters.Shape[2] = 0;
  CHECK(!BuildFieldSurface(parameters, heights), "zero patch stride cannot enter the patch loop");
  parameters.Shape[2] = 2;
  parameters.OriginPitch[3] = std::numeric_limits<float>::max();
  CHECK(!BuildFieldSurface(parameters, heights), "overflowing positions cannot reach the GPU");
  parameters.OriginPitch[3] = 10;
  parameters.Coverage[0] = 1.1f;
  CHECK(!BuildFieldSurface(parameters, heights), "lots cannot extend beyond their cells");
  parameters.Coverage[0] = .75f;
  CHECK(!BuildFieldSurface(parameters, std::span(heights).first(5)),
        "the field cannot read a truncated height grid");
  return Report();
}
