#include "BuildingMeshConversion.h"
#include "BuildingMesh.h"
#include "BuildingPlaneCapture.h"
#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "Check.h"
#include "Geodesy.h"
#include "ImpostorBaker.h"
#include "Box.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr uint32_t kPixels = 64;

Vec3 Local(const Vec3 &point, const EnuAxes &frame) {
  return {{Dot(point, frame.East), Dot(point, frame.Up), -Dot(point, frame.North)}};
}

Vec3 Ecef(const Vec3 &point, const EnuAxes &frame) {
  return frame.East * point[0] + frame.Up * point[1] - frame.North * point[2];
}

std::optional<Geometry> GeometryOf(const Raised &raised, const EnuAxes &frame) {
  Geometry geometry;
  Material wall;
  wall.BaseColour = {{.74f, .71f, .65f, 1}};
  wall.Roughness = .88f;
  wall.Pattern =
      raised.WallMaterialM.empty() ? SurfacePattern::Facade : SurfacePattern::FacadeMetres;
  Material roof;
  roof.BaseColour = {{.42f, .20f, .14f, 1}};
  roof.Roughness = .72f;
  roof.Pattern = SurfacePattern::Facade;
  const std::array materials{wall, roof};
  const std::array parts{BuildingMeshPart{.Vertices = raised.WallCorners,
                                          .Indices = raised.WallRun,
                                          .Colours = raised.WallColours,
                                          .MaterialM = raised.WallMaterialM},
                         BuildingMeshPart{.Vertices = raised.RoofCorners,
                                          .Indices = raised.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    if (parts[at].Vertices.empty()) { continue; }
    auto mesh = ConvertBuildingMesh(parts[at], frame);
    const auto surface = geometry.addSurface("source", materials[at]);
    if (!mesh || !surface) { return std::nullopt; }
    const auto part = geometry.addPart("source", *surface);
    if (!part || !geometry.adoptMesh(*part, *mesh)) { return std::nullopt; }
  }
  return geometry;
}

std::optional<ImpostorAtlas>
Capture(const Raised &raised, const EnuAxes &frame, const Box &bounds) {
  auto geometry = GeometryOf(raised, frame);
  CHECK(geometry, "source material streams form native capture geometry");
  if (!geometry) { return std::nullopt; }
  Render::ImpostorCapture capture;
  capture.LeastM = bounds.Min;
  capture.MostM = bounds.Max;
  capture.ViewDirections = {{{0, 0, 1}}};
  capture.Sources.push_back({.Mesh = std::move(*geometry), .Instances = {Mat4{}}});
  std::string error;
  auto atlas =
      Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = kPixels, .Views = 1}, error);
  CHECK(atlas, error.c_str());
  return atlas;
}

void Compare(const ImpostorAtlas &candidate, const ImpostorAtlas &original) {
  CHECK(candidate.CentreM() == original.CentreM() &&
            candidate.HalfExtentM() == original.HalfExtentM(),
        "both native material captures use the identical fixed projection");
  size_t common = 0;
  size_t changedCoverage = 0;
  double baseError = 0;
  double normalError = 0;
  double roughnessError = 0;
  const auto &a = candidate.Views().front();
  const auto &b = original.Views().front();
  for (size_t at = 0; at < a.Texels.size(); ++at) {
    const bool coveredA = a.Texels[at].Surface != 0;
    const bool coveredB = b.Texels[at].Surface != 0;
    changedCoverage += coveredA != coveredB ? 1U : 0U;
    if (!coveredA || !coveredB) { continue; }
    ++common;
    for (size_t axis = 0; axis < 3; ++axis) {
      baseError = std::max(baseError,
                           static_cast<double>(std::abs(a.Materials[at].BaseColour[axis] -
                                                        b.Materials[at].BaseColour[axis])));
      normalError = std::max(
          normalError,
          static_cast<double>(std::abs(a.Texels[at].Normal[axis] - b.Texels[at].Normal[axis])));
    }
    roughnessError = std::max(
        roughnessError,
        static_cast<double>(std::abs(a.Materials[at].Roughness - b.Materials[at].Roughness)));
  }
  if (changedCoverage != 0 || baseError >= .005 || normalError >= .003 || roughnessError >= .003) {
    std::cerr << "material_capture common=" << common << " coverage=" << changedCoverage
              << " base=" << baseError << " normal=" << normalError
              << " roughness=" << roughnessError << '\n';
  }
  CHECK(common > 100, "the material comparison contains an actual visible facade");
  CHECK(changedCoverage == 0, "pixel-centre plane sampling preserves this opaque facade coverage");
  CHECK(baseError < .005 && normalError < .003 && roughnessError < .003,
        "sampled source patches preserve native procedural paint, normals and roughness");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const std::array<double, 8> ring{48, 16, 48, 16.0003, 48.0002, 16.0003, 48.0002, 16};
    StructurePlan plan;
    plan.RingLatLon = ring;
    plan.BaseAslM = plan.SeatAslM = plan.FootAslM = 300;
    plan.HeightM = 12;
    plan.HeightMeasured = true;
    plan.PitchedShare = 0;
    plan.Coarseness = LevelOfDetail::Shell;
    plan.WallColour = Vec3f{{.4f, .7f, .3f}};
    plan.Facade = FacadeStyle::House;
    GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 300}, plan.AnchorEcef);
    BuildingScratch scratch;
    const auto source = BuildingSurface::Prepare(plan, scratch);
    CHECK(source, "analytical building forms prepare before any reference shell");
    if (!source) { return Report(); }
    const auto &frame = source->Axes();
    Box bounds;
    for (unsigned corner = 0; corner < 8; ++corner) {
      bounds.Cover(Local(source->Bounds().Corner(corner), frame));
    }
    const Vec3 half = bounds.Span() * .5;
    const double extent = std::max({half[0], half[1], half[2] * .5}) * kPixels / (kPixels - 2);
    const std::array<const BuildingSurface *, 1> sources{&*source};
    const auto producer = BuildingPlaneCapture::Prepare(sources);
    CHECK(producer, "source hierarchy contains only existing analytical forms");
    if (!producer) { return Report(); }
    const auto sampled = producer->Capture(
        {.Centre = Ecef(bounds.Middle(), frame),
         .TowardEye = frame.North * -1.0,
         .HalfExtentM = extent,
         .Pixels = kPixels,
         .Frame = ImpostorViewFrame{.Right = frame.East, .Up = frame.Up}},
        {.Samples = uint64_t{kPixels} * kPixels, .SourceTests = uint64_t{kPixels} * kPixels});
    CHECK(sampled && sampled->Covered > 100, "source-only producer returns visible native patches");
    if (!sampled) { return Report(); }
    const auto candidate = Capture(sampled->Mesh, frame, bounds);
    Raised reference;
    const BuildingMesh mesher;
    CHECK(mesher.Mesh(plan, scratch, reference),
          "independent native reference shell builds afterwards");
    const auto original = Capture(reference, frame, bounds);
    if (candidate && original) { Compare(*candidate, *original); }
  }
  SDL_Quit();
  return Report();
}
