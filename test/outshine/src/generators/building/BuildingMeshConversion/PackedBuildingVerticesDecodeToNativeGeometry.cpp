#include "BuildingMeshConversion.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array vertices{
      StoredVertex::Of(Vec3f{{0, 0, 0}}, Vec2f{{1, 2}}, Vec3f{{1, 0, 0}}),
      StoredVertex::Of(Vec3f{{2, 0, 0}}, Vec2f{{5.25f, -7.5f}}, Vec3f{{0, 1, 0}}),
      StoredVertex::Of(Vec3f{{0, 2, 0}}, Vec2f{{5632.25f, 4097.5f}}, Vec3f{{0, 0, 1}})};
  const EnuAxes frame{.East = {{0, 1, 0}}, .North = {{0, 0, 1}}, .Up = {{1, 0, 0}}};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  const auto mesh = Generators::ConvertBuildingMesh(
      {.Vertices = vertices, .Indices = indices, .Colours = {}, .MaterialM = {}}, frame);
  CHECK(mesh, "typed packed vertices and indices accepted");
  CHECK(!Generators::ConvertBuildingMesh({.Vertices = std::span(vertices).first(2),
                                          .Indices = indices,
                                          .Colours = {},
                                          .MaterialM = {}},
                                         frame),
        "incomplete vertex input is rejected");
  if (!mesh) { return Report(); }
  const std::array<float, 9> positions{0, 0, 0, 0, 2, 0, 2, 0, 0};
  const std::array<float, 9> normals{0, 1, 0, 1, 0, 0, 0, 0, -1};
  const std::array<float, 6> texture{1, 2, 5.25f, -7.5f, 5632.25f, 4097.5f};
  CHECK(std::ranges::equal(mesh->PositionsM, positions),
        "ECEF relative positions become East/Up/South");
  // Quantization of [-1,1] into 65536 values has half-step 1/65535.
  constexpr float halfStep = 1.0f / std::numeric_limits<uint16_t>::max();
  constexpr float rounding = 8.0f * std::numeric_limits<float>::epsilon();
  CHECK(std::ranges::equal(
            mesh->Normals,
            normals,
            [](float a, float b) { return std::abs(a - b) <= 2.0f * halfStep + rounding; }),
        "packed normals decoded through their representation");
  CHECK(std::ranges::equal(mesh->Uv, texture),
        "native texture coordinates retain signed and facade-scale values exactly");
  CHECK(std::ranges::equal(mesh->Indices, indices), "triangle ordering retained");

  const std::array quad{vertices[0],
                        vertices[1],
                        vertices[2],
                        StoredVertex::Of(Vec3f{{2, 2, 0}}, Vec2f{{3, 4}}, Vec3f{{0, 0, 1}})};
  const std::array<uint32_t, 6> run{0, 1, 2, 1, 3, 2};
  const std::array<float, 16> colours{
      .2f, .3f, .4f, 1, .5f, .6f, .7f, 1, .8f, .9f, 1, 1, .1f, .2f, .3f, 1};
  const std::array<float, 8> metres{-3, 7, 11, 13, 17, -19, 23, 29};
  const auto patch = Generators::ConvertBuildingMesh(
      {.Vertices = quad, .Indices = run, .Colours = colours, .MaterialM = metres}, frame);
  CHECK(patch, "indexed plane patch retains its independent material streams");
  CHECK(!Generators::ConvertBuildingMesh(
            {.Vertices = vertices, .Indices = run, .Colours = {}, .MaterialM = {}}, frame),
        "out-of-range indices are rejected");
  CHECK(!Generators::ConvertBuildingMesh({.Vertices = quad,
                                          .Indices = run,
                                          .Colours = std::span(colours).first(4),
                                          .MaterialM = {}},
                                         frame),
        "partial material attributes are rejected");
  if (patch) {
    CHECK(patch->PositionsM.size() == 12 && std::ranges::equal(patch->Indices, run),
          "a quad remains four shared vertices and two triangles, without soup expansion");
    CHECK(std::ranges::equal(patch->Colours, colours) && std::ranges::equal(patch->Uv1, metres),
          "source paint and signed material metres reach native Geometry exactly");
  }
  return Report();
}
