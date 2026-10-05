#include "Check.h"
#include "Geodesy.h"
#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/FacadeUv.h"
#include <array>
#include <cmath>

namespace {
using namespace outshine;

Vec3 Position(const StoredVertex &vertex) {
  return {{vertex.pos[0], vertex.pos[1], vertex.pos[2]}};
}

double Volume(std::span<const StoredVertex> vertices, std::span<const uint32_t> indices) {
  double volume = 0;
  for (size_t at = 0; at < indices.size(); at += 3) {
    volume += Dot(Position(vertices[indices[at]]),
                  Cross(Position(vertices[indices[at + 1]]), Position(vertices[indices[at + 2]]))) /
              6.0;
  }
  return volume;
}

double Volume(const Raised &mesh) {
  return Volume(mesh.WallCorners, mesh.WallRun) + Volume(mesh.RoofCorners, mesh.RoofRun);
}

double GlassArea(const Raised &mesh) {
  double area = 0;
  for (size_t at = 0; at < mesh.WallRun.size(); at += 3) {
    const auto &a = mesh.WallCorners[mesh.WallRun[at]];
    const float code = std::fmod(-a.uv()[0] - 1.0f, 16.0f);
    if (!IsGlazingUv(a.uv()[0]) &&
        (a.uv()[0] >= 0.0f || code != static_cast<float>(Facade::Glass))) {
      continue;
    }
    const auto &b = mesh.WallCorners[mesh.WallRun[at + 1]];
    const auto &c = mesh.WallCorners[mesh.WallRun[at + 2]];
    area += 0.5 * Length(Cross(Position(b) - Position(a), Position(c) - Position(a)));
  }
  return area;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<double, 8> ring{47, 9, 47, 9.00015, 47.0001, 9.00015, 47.0001, 9};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 9;
  plan.HeightMeasured = true;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 0}, plan.AnchorEcef);
  Generators::BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  Raised fine;
  CHECK(mesher.Mesh(plan, *scratch, fine).has_value(), "Fine housing meshes");
  plan.Coarseness = LevelOfDetail::Shell;
  Raised shell;
  CHECK(mesher.Mesh(plan, *scratch, shell).has_value(), "Shell housing meshes");
  const double glass = GlassArea(fine);
  CHECK(glass > 1.0 && GlassArea(shell) == 0.0,
        "only the near mesh expands planned housing glass into geometry");
  for (const auto &vertex : fine.WallCorners) {
    const auto uv = vertex.uv();
    if (!IsGlazingUv(uv[0])) { continue; }
    const float bay = std::fmod(uv[0], 1.0f);
    const float storey = std::fmod(uv[1], 1.0f);
    CHECK(std::abs(bay - 0.27f) < 0.003f || std::abs(bay - 0.73f) < 0.003f,
          "glass retains the same opening columns as the shell");
    CHECK(std::abs(storey - 0.28f) < 0.003f || std::abs(storey - 0.78f) < 0.003f,
          "glass retains the same opening rows as the shell");
  }
  const double removed = std::abs(Volume(shell)) - std::abs(Volume(fine));
  CHECK_NEAR(removed,
             0.16 * glass,
             std::max(0.05, 0.01 * 0.16 * glass),
             "m3",
             "the closed wall loses exactly glass area times recess depth");
  CHECK(fine.RoofRun.size() == shell.RoofRun.size(), "opening detail preserves the roof");
  return Report();
}
