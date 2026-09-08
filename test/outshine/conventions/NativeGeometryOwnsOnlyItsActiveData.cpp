#include <array>
#include <limits>
#include <scene/Geometry.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const std::array<float, 9> positions{0, 0, 0, 1, 0, 0, 0, 1, 0};
  const std::array<uint32_t, 3> triangle{0, 1, 2};
  const auto add = [&] {
    const int part = geometry.addPart("triangle", MaterialInstance{});
    CHECK(geometry.setPositions(part, positions), "copy local positions");
    CHECK(geometry.setTriangles(part, triangle), "copy triangle indices");
    return part;
  };
  (void)add();
  (void)add();
  CHECK(geometry.wellFormed(), "two active triangles are valid");
  CHECK(geometry.addImage(1, 1, std::array<uint8_t, 4>{255, 255, 255, 255}) == 0,
        "the first image has index zero");
  geometry.clear();
  CHECK(geometry.parts() == 0 && geometry.images() == 0 && geometry.surfaces() == 0 &&
            geometry.lamps() == 0 && !geometry.wellFormed(),
        "clear removes all active content");
  const int part = add();
  CHECK(part == 0 && geometry.wellFormed(),
        "a smaller rebuild is independent of retained part capacity");
  CHECK(geometry.addImage(1, 1, std::array<uint8_t, 4>{0, 0, 0, 255}) == 0,
        "images from the previous build do not survive clear");

  const std::array<uint8_t, 4> pixel{17, 31, 63, 255};
  const int imageCount = geometry.images();
  for (const auto dimensions :
       {std::array<int, 2>{0, 1},
        std::array<int, 2>{1, 0},
        std::array<int, 2>{-1, 1},
        std::array<int, 2>{1, -1},
        std::array<int, 2>{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}}) {
    CHECK(geometry.addImage(dimensions[0], dimensions[1], pixel) == -1,
          "invalid or unbacked image dimensions are rejected before allocation");
  }
  CHECK(geometry.addImage(1, 1, {}) == -1 &&
            geometry.addImage(1, 1, std::array<uint8_t, 3>{}) == -1 &&
            geometry.addImage(1, 1, std::array<uint8_t, 5>{}) == -1,
        "RGBA8 requires the exact byte count, without truncation or trailing data");
  CHECK(geometry.images() == imageCount && geometry.imageAt(0).Rgba[3] == 255,
        "rejected images preserve existing content and index allocation");
  CHECK(geometry.addImage(1, 1, pixel) == imageCount &&
            geometry.imageAt(imageCount).Rgba.size() == pixel.size() &&
            geometry.imageAt(imageCount).Rgba[0] == pixel[0],
        "the next valid image owns exactly its declared pixels");

  for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) {
    auto bad = positions;
    bad[4] = invalid;
    CHECK(!geometry.setPositions(part, bad) && geometry.positionsOf(part)[4] == positions[4],
          "nonfinite positions are rejected without replacing valid data");
    CHECK(!geometry.setNormals(part, std::array<float, 3>{invalid, 0, 1}) &&
              geometry.normalsOf(part).empty(),
          "nonfinite normals are rejected without mutation");
    CHECK(!geometry.setTexture(part, std::array<float, 2>{0, invalid}) &&
              geometry.textureOf(part).empty(),
          "nonfinite UVs are rejected without mutation");
    CHECK(!geometry.setTangents(part, std::array<float, 4>{1, 0, 0, invalid}) &&
              geometry.tangentsOf(part).empty(),
          "nonfinite tangents are rejected without mutation");
    CHECK(!geometry.setColours(part, std::array<float, 4>{1, invalid, 1, 1}) &&
              geometry.coloursOf(part).empty(),
          "nonfinite colours are rejected without mutation");
  }
  CHECK(geometry.setNormals(part, std::array<float, 3>{0, 0, -1}),
        "partial attributes may be supplied while assembling");
  for (const auto indices : {std::array<uint32_t, 3>{0, 1, 2},
                             std::array<uint32_t, 3>{1, 2, 0},
                             std::array<uint32_t, 3>{2, 0, 1}}) {
    CHECK(geometry.setTriangles(part, indices), "rotate corner order");
    CHECK(geometry.windingAgainstNormals(part) == 0,
          "incomplete normals are safe at every corner and not a winding verdict");
  }
  CHECK(!geometry.wellFormed(), "incomplete attributes cannot be published");
  CHECK(geometry.setNormals(part, std::array<float, 9>{0, 0, -1, 0, 0, -1, 0, 0, -1}),
        "complete opposing normals");
  CHECK(geometry.windingAgainstNormals(part) == 1, "complete input detects reversed winding");
  Geometry copied = geometry.clone();
  CHECK(copied.wellFormed() && copied.parts() == geometry.parts(),
        "clone contains only the active native parts");
  CHECK(copied.positionsOf(part).data() != geometry.positionsOf(part).data(),
        "clone owns separate vertex storage");
  CHECK(copied.imageAt(0).Rgba.data() != geometry.imageAt(0).Rgba.data(),
        "clone owns separate image storage");
  geometry.clear();
  CHECK(copied.wellFormed() && copied.images() == imageCount + 1 &&
            copied.imageAt(imageCount).Rgba[0] == pixel[0],
        "source clear preserves the independent clone and its image indices");
  geometry.clear();
  CHECK(geometry.parts() == 0 && geometry.images() == 0 && !geometry.wellFormed(),
        "repeated clear leaves an empty owner");
  return Report();
}
