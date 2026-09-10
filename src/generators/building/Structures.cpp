#include <string_view>
#include <charconv>
#include <expected>
#include <system_error>
#include <span>
#include "Structures.h"

#include "ground/TileMeshes.h"

#include <array>
#include <cmath>

#include "math/Vec3.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "BuildingMesh.h"
#include "Meshed.h"

namespace outshine::Generators {

namespace Says {
constexpr auto kInvalidStructureWidth = "structures accepts one finite positive widthM parameter";
}

constexpr uint64_t kSplitMixOffset = 1442695040888963407ull;

namespace {

[[nodiscard]] std::expected<double, std::string_view>
WidthOf(std::span<const Parameter> parameters) {
  constexpr double defaultWidthM = 12.0;
  if (parameters.empty()) { return defaultWidthM; }
  if (parameters.size() != 1 || parameters.front().Name != "widthM") {
    return std::unexpected(Says::kInvalidStructureWidth);
  }
  const std::string_view text = parameters.front().Value;
  if (text.empty()) { return std::unexpected(Says::kInvalidStructureWidth); }
  double widthM = 0.0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), widthM);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
      !std::isfinite(widthM) || widthM <= 0.0) {
    return std::unexpected(Says::kInvalidStructureWidth);
  }
  return widthM;
}

constexpr uint64_t kSplitMixWord = 6364136223846793005ull;
constexpr uint64_t kSplitMixFinaliser = 0xff51afd7ed558ccdull;
constexpr unsigned kSplitMixShift = 33u;
constexpr uint64_t kMantissaMask = 0xFFFFFFull;

constexpr double kHeightLeastM = 6.0;
constexpr double kHeightSwingM = 12.0;

constexpr float kWallRed = 0.62f;
constexpr float kWallGreen = 0.60f;
constexpr float kWallBlue = 0.56f;
constexpr float kWallRoughness = 0.85f;

constexpr size_t kCorners = 4;

[[nodiscard]] double Spun(uint64_t seed, uint32_t at) {
  uint64_t held = seed * kSplitMixWord + static_cast<uint64_t>(at) * kSplitMixOffset + 1u;
  held ^= held >> kSplitMixShift;
  held *= kSplitMixFinaliser;
  held ^= held >> kSplitMixShift;
  return static_cast<double>(held & kMantissaMask) / static_cast<double>(kMantissaMask);
}

}

bool Structures::make(const Request &asked, Geometry &into) const {
  const auto width = WidthOf(asked.Parameters);
  if (!width) { return false; }
  const double sideM = *width;
  const double lat = asked.LatitudeDeg;
  const double lon = asked.LongitudeDeg;
  const double halfLatDeg = 0.5 * sideM / kMPerDegLat;
  const double halfLonDeg = 0.5 * sideM / (kMPerDegLon * std::cos(lat * kDeg2Rad));

  const std::array<double, kCorners * 2> ring = {{lat - halfLatDeg,
                                                  lon - halfLonDeg,
                                                  lat - halfLatDeg,
                                                  lon + halfLonDeg,
                                                  lat + halfLatDeg,
                                                  lon + halfLonDeg,
                                                  lat + halfLatDeg,
                                                  lon - halfLonDeg}};
  const std::array<double, kCorners> corners = {{0.0, 0.0, 0.0, 0.0}};
  const Vec3 anchor;

  StructurePlan plan;
  plan.RingLatLon = std::span<const double>(ring.data(), kCorners * 2);
  plan.CornerAslM = std::span<const double>(corners.data(), kCorners);
  plan.BaseAslM = 0.0;
  plan.HeightM = kHeightLeastM + kHeightSwingM * Spun(asked.Seed, 1u);
  plan.HeightMeasured = false;
  plan.AnchorEcef = anchor;

  const BuildingMesh mesher;
  const std::unique_ptr<MeshScratch> scratch = mesher.Scratch();
  Raised raised;
  if (!mesher.Mesh(plan, *scratch, raised)) { return false; }
  std::vector<StoredVertex> soup;
  const auto spread = [&soup](const std::vector<StoredVertex> &corners,
                              const std::vector<uint32_t> &run) {
    for (const uint32_t corner : run) { soup.push_back(corners[corner]); }
  };
  spread(raised.WallCorners, raised.WallRun);
  spread(raised.RoofCorners, raised.RoofRun);
  if (soup.empty()) { return false; }

  Meshed made;
  if (!made.Take("structure", MaterialInstance(0), soup)) { return false; }
  const Geometry stood = made.Handed();
  if (stood.parts() == 0) { return false; }

  Material walls;
  walls.BaseColour[0] = kWallRed;
  walls.BaseColour[1] = kWallGreen;
  walls.BaseColour[2] = kWallBlue;
  walls.Roughness = kWallRoughness;
  const MaterialInstance named = into.addSurface("walls", walls);
  for (int part = 0; part < stood.parts(); ++part) {
    const int here = into.addPart("structure", named);
    if (!into.setPositions(here, stood.positionsOf(part)) ||
        !into.setTriangles(here, stood.trianglesOf(part))) {
      return false;
    }
    if (!stood.normalsOf(part).empty()) { (void)into.setNormals(here, stood.normalsOf(part)); }
    if (!stood.textureOf(part).empty()) { (void)into.setTexture(here, stood.textureOf(part)); }
  }
  return true;
}

}
