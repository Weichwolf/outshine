#ifndef OUTSHINE_TEST_BUILDING_VIEW_GEOMETRY_H
#define OUTSHINE_TEST_BUILDING_VIEW_GEOMETRY_H

#include "StructureMesher.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Test {
template <typename T> bool ViewBytesEqual(const std::vector<T> &a, const std::vector<T> &b) {
  return std::ranges::equal(std::as_bytes(std::span(a)), std::as_bytes(std::span(b)));
}

inline bool ViewGeometryEqual(const Raised &a, const Raised &b) {
  return ViewBytesEqual(a.WallCorners, b.WallCorners) &&
         ViewBytesEqual(a.RoofCorners, b.RoofCorners) && ViewBytesEqual(a.WallRun, b.WallRun) &&
         ViewBytesEqual(a.RoofRun, b.RoofRun) && ViewBytesEqual(a.WallColours, b.WallColours) &&
         ViewBytesEqual(a.WallMaterialM, b.WallMaterialM);
}

inline nlohmann::json ViewGeometryStream(std::span<const StoredVertex> vertices,
                                         std::span<const uint32_t> indices) {
  auto positions = nlohmann::json::array();
  auto normals = nlohmann::json::array();
  auto uv = nlohmann::json::array();
  for (const auto &vertex : vertices) {
    positions.push_back({vertex.pos[0], vertex.pos[1], vertex.pos[2]});
    normals.push_back(vertex.normWord);
    uv.push_back({vertex.texture[0], vertex.texture[1]});
  }
  auto triangles = nlohmann::json::array();
  for (size_t at = 0; at + 2 < indices.size(); at += 3) {
    triangles.push_back({indices[at], indices[at + 1], indices[at + 2]});
  }
  return {{"positions", std::move(positions)},
          {"normal_words", std::move(normals)},
          {"uv", std::move(uv)},
          {"triangles", std::move(triangles)}};
}

inline nlohmann::json ViewGeometryInputs(const Raised &mesh) {
  return {{"walls", ViewGeometryStream(mesh.WallCorners, mesh.WallRun)},
          {"roofs", ViewGeometryStream(mesh.RoofCorners, mesh.RoofRun)},
          {"wall_colours", mesh.WallColours},
          {"wall_material_metres", mesh.WallMaterialM}};
}
}
#endif
