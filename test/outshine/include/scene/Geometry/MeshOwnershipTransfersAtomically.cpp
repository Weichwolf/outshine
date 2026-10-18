#include <scene/Geometry.h>
#include <content/GeometryAsset.h>
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry::Mesh Triangle() {
  return {.PositionsM = {0, 0, 0, 1, 0, 0, 0, 1, 0},
          .Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1},
          .Uv = {0, 0, 1, 0, 0, 1},
          .Uv1 = {1, 1, 0, 1, 1, 0},
          .Tangents = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1},
          .Colours = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1},
          .Indices = {0, 1, 2}};
}

std::array<const void *, 7> Pointers(const Geometry::Mesh &mesh) {
  return {mesh.PositionsM.data(),
          mesh.Normals.data(),
          mesh.Uv.data(),
          mesh.Uv1.data(),
          mesh.Tangents.data(),
          mesh.Colours.data(),
          mesh.Indices.data()};
}

std::array<const void *, 7> Pointers(const Geometry &geometry) {
  return {geometry.positionsOf(0).data(),
          geometry.normalsOf(0).data(),
          geometry.textureOf(0).data(),
          geometry.textureOf(0, UvSet::Uv1).data(),
          geometry.tangentsOf(0).data(),
          geometry.coloursOf(0).data(),
          geometry.trianglesOf(0).data()};
}

template <typename T> bool Same(const std::vector<T> &a, const std::vector<T> &b) {
  return a.size() == b.size() &&
         (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

bool Same(const Geometry::Mesh &a, const Geometry::Mesh &b) {
  return Same(a.PositionsM, b.PositionsM) && Same(a.Normals, b.Normals) && Same(a.Uv, b.Uv) &&
         Same(a.Uv1, b.Uv1) && Same(a.Tangents, b.Tangents) && Same(a.Colours, b.Colours) &&
         Same(a.Indices, b.Indices);
}

void Rejection(Geometry &geometry, Geometry::Mesh mesh, GeometryAttributeError expected) {
  constexpr size_t kLimit = 4096;
  const auto input = Pointers(mesh);
  const auto saved = mesh;
  const auto oldPointers = Pointers(geometry);
  const auto before = EncodeGeometryAsset(geometry, kLimit);
  const auto result = geometry.adoptMesh(0, mesh);
  CHECK(!result && result.error() == expected, "invalid mesh reports its attribute failure");
  CHECK(Pointers(mesh) == input && Same(mesh, saved),
        "failed transfer changes no input stream or value, including NaN bits");
  CHECK(Pointers(geometry) == oldPointers && EncodeGeometryAsset(geometry, kLimit) == before,
        "failed transfer preserves every published stream and its values");
}

void RejectInvalidMeshes(Geometry &geometry) {
  for (size_t at = 0; at < 6; ++at) {
    auto mesh = Triangle();
    const std::array streams{
        &mesh.PositionsM, &mesh.Normals, &mesh.Uv, &mesh.Uv1, &mesh.Tangents, &mesh.Colours};
    streams[at]->pop_back();
    Rejection(geometry, std::move(mesh), GeometryAttributeError::IncompleteTuple);
  }
  auto mesh = Triangle();
  mesh.Normals[2] = 2;
  Rejection(geometry, std::move(mesh), GeometryAttributeError::NonUnitNormal);
  mesh = Triangle();
  mesh.Colours[0] = std::numeric_limits<float>::infinity();
  Rejection(geometry, std::move(mesh), GeometryAttributeError::NonFiniteValue);
  mesh = Triangle();
  mesh.PositionsM[0] = std::numeric_limits<float>::quiet_NaN();
  Rejection(geometry, std::move(mesh), GeometryAttributeError::NonFiniteValue);
  mesh = Triangle();
  mesh.Indices.pop_back();
  Rejection(geometry, std::move(mesh), GeometryAttributeError::IncompleteTuple);
}
}

int main() {
  constexpr double kPlacementM = 8;
  Geometry geometry;
  const auto surface = geometry.addSurface("surface", Material{});
  CHECK(surface.has_value(), "the target owns its material");
  if (!surface) { return Report(); }
  const auto part = geometry.addPart("mesh", *surface);
  CHECK(part.has_value(), "the target owns its part");
  if (!part) { return Report(); }
  Mat4 placement;
  placement.Column[12] = kPlacementM;
  CHECK(geometry.setPlacement(*part, placement), "the target has an independent placement");
  auto mesh = Triangle();
  const auto input = Pointers(mesh);
  const auto result = geometry.adoptMesh(*part, mesh);
  CHECK(result && Pointers(geometry) == input,
        "successful transfer retains all seven allocations without payload copies");
  CHECK(geometry.wellFormed() && geometry.materialOf(0) == *surface &&
            geometry.placementOf(0).Column == placement.Column && geometry.nameOf(0) == "mesh",
        "mesh transfer preserves metadata and publishes complete geometry");
  const auto copy = geometry.clone();
  CHECK(Pointers(copy) != Pointers(geometry) && copy.wellFormed(),
        "explicit cloning still creates independently owned streams");
  RejectInvalidMeshes(geometry);
  mesh = Triangle();
  mesh.Indices[2] = static_cast<uint32_t>(mesh.PositionsM.size() / 3);
  CHECK(geometry.adoptMesh(0, mesh) && !geometry.wellFormed(),
        "index bounds remain checked at the same complete-product boundary");
  CHECK(geometry.setTriangles(0, copy.trianglesOf(0)) && geometry.wellFormed(),
        "the existing copying setters remain usable after a transfer");
  mesh = Triangle();
  const auto preserved = Pointers(mesh);
  const auto missing = geometry.adoptMesh(-1, mesh);
  CHECK(!missing && missing.error() == GeometryAttributeError::MissingPart &&
            Pointers(mesh) == preserved,
        "an absent target leaves the supplied mesh owned by its caller");
  Geometry::Mesh empty;
  CHECK(geometry.adoptMesh(0, empty) && geometry.positionsOf(0).empty() &&
            geometry.trianglesOf(0).empty() && geometry.nameOf(0) == "mesh",
        "an empty transfer clears streams while retaining the part");
  geometry.clear();
  CHECK(!geometry.adoptMesh(0, mesh) && Pointers(mesh) == preserved,
        "a retained inactive slot is not a valid transfer target");
  return Report();
}
