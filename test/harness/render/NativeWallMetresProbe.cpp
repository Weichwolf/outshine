#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "BuildingWallMaterial.h"
#include "StructureArtifact.h"
#include "Sha256.h"
#include "Check.h"
#include "Geodesy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <print>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kLatitudeDeg = 48.2;
constexpr double kLongitudeDeg = 16.37;
constexpr double kWidthDeg = 0.0003;
constexpr double kDepthDeg = 0.0002;
constexpr double kAslM = 100;
constexpr double kMetricToleranceM = 0.0002;
constexpr size_t kSearchCases = 128;
constexpr Vec3f kExplicitColour{{0.4f, 0.3f, 0.2f}};

Vec3 Local(const BuildingSurface &source, const Vec3f &point) {
  const Vec3 relative = Vec3{{point[0], point[1], point[2]}} - source.Origin();
  return {{Dot(relative, source.Axes().East),
           Dot(relative, source.Axes().North),
           Dot(relative, source.Axes().Up)}};
}

double MeshError(const BuildingSurface &source, const Raised &mesh) {
  CHECK(mesh.WallMaterialM.size() == mesh.WallCorners.size(),
        "the optional metre stream covers every wall vertex once present");
  if (mesh.WallMaterialM.size() != mesh.WallCorners.size()) { return INFINITY; }
  const auto &shape = source.Shapes().front();
  double maximum = 0.0;
  for (size_t at = 0; at < mesh.WallCorners.size(); ++at) {
    const auto &vertex = mesh.WallCorners[at];
    const auto metric = mesh.WallMaterialM[at];
    if (vertex.texture[0] < 0.0f) {
      CHECK(metric[0] == 0.0f && metric[1] == 0.0f,
            "negative trim/roof/plinth codes receive no masonry coordinates");
      continue;
    }
    const Vec3 local = Local(source, vertex.pos);
    const Vec3f normal = vertex.norm();
    const Vec3 worldNormal{{normal[0], normal[1], normal[2]}};
    const double east = Dot(worldNormal, source.Axes().East);
    const double north = Dot(worldNormal, source.Axes().North);
    double nearest = INFINITY;
    double expectedU = INFINITY;
    for (size_t edge = 0; edge < shape.Ring.size(); ++edge) {
      const auto &a = shape.Ring[edge];
      const auto &b = shape.Ring[(edge + 1) % shape.Ring.size()];
      const double e = b.EastM - a.EastM;
      const double n = b.NorthM - a.NorthM;
      const double length = std::hypot(e, n);
      constexpr double kAlignedNormal = 0.99;
      if ((east * n - north * e) / length < kAlignedNormal) { continue; }
      const double distance =
          std::abs((local[0] - a.EastM) * n - (local[1] - a.NorthM) * e) / length;
      if (distance >= nearest) { continue; }
      nearest = distance;
      expectedU = ((local[0] - a.EastM) * e + (local[1] - a.NorthM) * n) / length;
    }
    maximum = std::max({maximum,
                        std::abs(expectedU - metric[0]),
                        std::abs(local[2] - shape.SeatM - shape.FootM - metric[1])});
  }
  CHECK(maximum < kMetricToleranceM,
        "emitted metres match the actual float geometry and independent source edges");
  return maximum;
}

void Cache(const Raised &mesh) {
  if (mesh.WallMaterialM.empty()) {
    CHECK(false, "the masonry cache probe needs emitted material coordinates");
    return;
  }
  BakedTile product;
  product.Built = mesh;
  const auto key = Sha256Hex("wall-material-metre-fixture");
  const auto encoded = EncodeStructureArtifact(product, key);
  CHECK(encoded.has_value(), "the actual complete building asset accepts valid metric coordinates");
  if (!encoded) { return; }
  auto decoded = DecodeStructureArtifact(*encoded, key, 1);
  CHECK(decoded.has_value(), "a fresh asset decode includes the metric stream");
  if (!decoded) { return; }
  CHECK(decoded->Built.WallMaterialM == mesh.WallMaterialM,
        "the exact metre values survive the versioned cache boundary");
  CHECK(decoded->Built.WallCorners.size() == mesh.WallCorners.size() &&
            std::memcmp(decoded->Built.WallCorners.data(),
                        mesh.WallCorners.data(),
                        mesh.WallCorners.size() * sizeof(StoredVertex)) == 0 &&
            decoded->Built.WallRun == mesh.WallRun,
        "cache roundtrip preserves original geometry and packed facade UVs");
  product.Built.WallMaterialM.clear();
  const auto without = EncodeStructureArtifact(product, key);
  CHECK(without && encoded->size() - without->size() == mesh.WallMaterialM.size() * sizeof(Vec2f),
        "the actual encoded cost is exactly eight additional bytes per covered wall vertex");
  product.Built.WallMaterialM = mesh.WallMaterialM;
  product.Built.WallMaterialM.pop_back();
  CHECK(!EncodeStructureArtifact(product, key),
        "a partial metric stream cannot become a cache hit");
  product.Built.WallMaterialM = mesh.WallMaterialM;
  product.Built.WallMaterialM.front()[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!EncodeStructureArtifact(product, key),
        "nonfinite material coordinates cannot enter the asset");
}

void Fixture(const char *prefix,
             double anchorM,
             const BuildingSurface &source,
             BuildingScratch &scratch) {
  if (prefix == nullptr) { return; }
  const BuildingSurface::Face face{.Part = 0, .Side = 2};
  BakedTile product;
  CHECK(source.MeshVisible(std::span(&face, 1), scratch, product.Built),
        "the raster fixture uses an actual emitted native source face");
  const auto key = Sha256Hex("actual-wall-uv1-raster");
  const auto encoded = EncodeStructureArtifact(product, key);
  if (!encoded) {
    CHECK(false, "the real raster asset encodes");
    return;
  }
  const auto decoded = DecodeStructureArtifact(*encoded, key, 1);
  if (!decoded) {
    CHECK(false, "the real raster asset decodes");
    return;
  }
  const auto &mesh = decoded->Built;
  if (mesh.WallRun.size() < 6 || mesh.WallMaterialM.size() != mesh.WallCorners.size()) {
    CHECK(false, "the decoded source face supplies both complete UV streams");
    return;
  }
  const std::array frame{
      source.Origin(), source.Axes().East, source.Axes().Up, source.Axes().North};
  const auto &shape = source.Shapes().front();
  const std::array edge{
      Vec3{{shape.Ring[0].EastM, shape.SeatM + shape.FootM, shape.Ring[0].NorthM}},
      Vec3{{shape.Ring[1].EastM, shape.SeatM + shape.FootM, shape.Ring[1].NorthM}}};
  const std::string path =
      std::string(prefix) + "-" + std::to_string(static_cast<int>(anchorM)) + ".bin";
  std::ofstream file(path, std::ios::binary);
  for (const auto &axis : frame) {
    for (const double value : axis) {
      file.write(reinterpret_cast<const char *>(&value), sizeof(value));
    }
  }
  for (const auto &point : edge) {
    for (const double value : point) {
      file.write(reinterpret_cast<const char *>(&value), sizeof(value));
    }
  }
  for (size_t at = 0; at < 6; ++at) {
    const auto &v = mesh.WallCorners[mesh.WallRun[at]];
    file.write(reinterpret_cast<const char *>(&v), sizeof(v));
  }
  for (size_t at = 0; at < 6; ++at) {
    const auto &uv = mesh.WallMaterialM[mesh.WallRun[at]];
    file.write(reinterpret_cast<const char *>(&uv), sizeof(uv));
  }
  CHECK(file, "the emitted cached wall and actual UV1 bytes are saved");
}

void Patches(const BuildingSurface &source) {
  const auto &shape = source.Shapes().front();
  std::vector<double> cuts;
  size_t walls = 0;
  size_t gables = 0;
  constexpr double kHalfPatchM = 0.02;
  const auto global = [&source](const Vec3 &p) {
    return source.Origin() + source.Axes().East * p[0] + source.Axes().North * p[1] +
           source.Axes().Up * p[2];
  };
  for (size_t edge = 0; edge < shape.Ring.size(); ++edge) {
    const auto &a = shape.Ring[edge];
    const auto &b = shape.Ring[(edge + 1) % shape.Ring.size()];
    const double e = b.EastM - a.EastM;
    const double n = b.NorthM - a.NorthM;
    const double length = std::hypot(e, n);
    const Vec3 along = source.Axes().East * (e / length) + source.Axes().North * (n / length);
    const Vec3 outward = source.Axes().East * (n / length) - source.Axes().North * (e / length);
    for (const double height : {shape.FloorM * 0.5, shape.EavesM + shape.RiseM * 0.5}) {
      const Vec3 centre = global({{(a.EastM + b.EastM) * 0.5,
                                   (a.NorthM + b.NorthM) * 0.5,
                                   shape.SeatM + shape.FootM + height}});
      const auto hit =
          source.Trace({.Origin = centre + outward, .Direction = outward * -1.0}, 0, 2, cuts);
      if (!hit || hit->Face < 2) { continue; }
      const auto point = [&source, &centre, &along](double u, double v) {
        return centre + along * u + source.Axes().Up * v;
      };
      const std::array split{BuildingSurfacePatch{.Sample = *hit,
                                                  .Corners = {point(-kHalfPatchM, -kHalfPatchM),
                                                              point(0, -kHalfPatchM),
                                                              point(0, kHalfPatchM),
                                                              point(-kHalfPatchM, kHalfPatchM)}},
                             BuildingSurfacePatch{.Sample = *hit,
                                                  .Corners = {point(0, -kHalfPatchM),
                                                              point(kHalfPatchM, -kHalfPatchM),
                                                              point(kHalfPatchM, kHalfPatchM),
                                                              point(0, kHalfPatchM)}}};
      Raised mesh;
      source.MeshPatches(split, mesh);
      CHECK(mesh.WallMaterialM.size() == 8, "both actual source patches emit their metric stream");
      if (mesh.WallMaterialM.size() != 8) { continue; }
      CHECK(mesh.WallMaterialM[1] == mesh.WallMaterialM[4] &&
                mesh.WallMaterialM[2] == mesh.WallMaterialM[7],
            "detail subdivision preserves shared-edge material phase exactly");
      MeshError(source, mesh);
      Cache(mesh);
      walls += hit->Gable ? 0u : 1u;
      gables += hit->Gable ? 1u : 0u;
    }
  }
  CHECK(walls > 0 && gables > 0, "the native patch comparison contains walls and actual gables");
}

void Exercise(StructurePlan plan, const BuildingSurface &source, BuildingScratch &scratch) {
  plan.Prepared = &source;
  const BuildingMesh mesher;
  Patches(source);
  for (const auto detail :
       {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed, LevelOfDetail::Skyline}) {
    plan.Coarseness = detail;
    Raised mesh;
    const auto built = mesher.Mesh(plan, scratch, mesh);
    CHECK(built, "the native mesh produces the complete metric stream");
    if (!built) { continue; }
    const double error = MeshError(source, mesh);
    Cache(mesh);
    std::println(
        R"({{"detail":{},"wall_vertices":{},"metres_bytes":{},"metres_capacity_bytes":{},"max_error_m":{:.17g}}})",
        static_cast<int>(detail),
        mesh.WallCorners.size(),
        mesh.WallMaterialM.size() * sizeof(Vec2f),
        mesh.WallMaterialM.capacity() * sizeof(Vec2f),
        error);
  }
  plan.Prepared = nullptr;
  plan.WallColour = kExplicitColour;
  Raised painted;
  CHECK(mesher.Mesh(plan, scratch, painted), "the explicitly painted source meshes");
  CHECK(painted.WallMaterialM.empty(),
        "explicit source colour retains priority over generated masonry");
  const size_t previous = painted.WallCorners.size();
  plan.Prepared = &source;
  plan.WallColour.reset();
  CHECK(mesher.Mesh(plan, scratch, painted), "masonry appends to an existing painted product");
  CHECK(painted.WallMaterialM.size() == painted.WallCorners.size() &&
            std::ranges::all_of(std::span(painted.WallMaterialM).first(previous),
                                [](const Vec2f &uv) { return uv[0] == 0 && uv[1] == 0; }),
        "optional stream activation pads the existing non-masonry prefix");
  const auto saved = painted.WallMaterialM;
  const auto savedVertices = painted.WallCorners.size();
  plan.HeightM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!mesher.Mesh(plan, scratch, painted), "invalid append is rejected");
  CHECK(painted.WallMaterialM == saved && painted.WallCorners.size() == savedVertices,
        "failed append preserves existing metric values and geometry");
}
}

int main(int argc, char *const *argv) {
  BuildingScratch scratch;
  for (size_t seed = 0; seed < kSearchCases; ++seed) {
    const double longitude = kLongitudeDeg + static_cast<double>(seed) * kWidthDeg;
    const std::array ring{kLatitudeDeg,
                          longitude,
                          kLatitudeDeg,
                          longitude + kWidthDeg,
                          kLatitudeDeg + kDepthDeg,
                          longitude + kWidthDeg,
                          kLatitudeDeg + kDepthDeg,
                          longitude};
    StructurePlan plan;
    plan.RingLatLon = ring;
    plan.BaseAslM = kAslM;
    plan.HeightM = 12;
    plan.HeightMeasured = true;
    plan.PitchedShare = 1;
    plan.Facade = FacadeStyle::House;
    GeoToEcef({.LongitudeDeg = longitude, .LatitudeDeg = kLatitudeDeg, .HeightM = kAslM},
              plan.AnchorEcef);
    const auto source = BuildingSurface::Prepare(plan, scratch);
    if (!source || source->Shapes().size() != 1 ||
        source->Shapes().front().Roof != RoofKind::Gable ||
        !UsesWallMaterialMetres(source->Shapes().front())) {
      continue;
    }
    Exercise(plan, *source, scratch);
    Fixture(argc == 2 ? argv[1] : nullptr, 0, *source, scratch);
    constexpr double kAnchorM = 500;
    plan.AnchorEcef =
        plan.AnchorEcef + source->Axes().East * kAnchorM + source->Axes().North * kAnchorM;
    const auto moved = BuildingSurface::Prepare(plan, scratch);
    CHECK(moved.has_value(), "the producer accepts another tile anchor");
    if (moved) {
      Exercise(plan, *moved, scratch);
      Fixture(argc == 2 ? argv[1] : nullptr, kAnchorM, *moved, scratch);
    }
    return Report();
  }
  CHECK(false, "the declared fixture search must produce the actual masonry recipe");
  return Report();
}
