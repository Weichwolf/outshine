#include "Structures.h"

#include <cmath>
#include <string>
#include <vector>

#include "BuildingMesh.h"
#include "Meshed.h"

namespace outshine::Generators {

namespace {

constexpr size_t kCorners = 4;
constexpr double kMetresPerDegree = 111320.0;

[[nodiscard]] double Spun(uint64_t seed, uint32_t at) {
  uint64_t held = seed * 6364136223846793005ull + (uint64_t)at * 1442695040888963407ull + 1u;
  held ^= held >> 33;
  held *= 0xff51afd7ed558ccdull;
  held ^= held >> 33;
  return (double)(held & 0xFFFFFFull) / (double)0xFFFFFF;
}

} // namespace

bool Structures::make(const Ask &ask, Geometry &into) const {
  const double sideM = ask.ExtentM > 0.0 && ask.ExtentM < 200.0 ? ask.ExtentM : 12.0;
  const double halfDeg = 0.5 * sideM / kMetresPerDegree;
  const double lat = ask.NorthM;
  const double lon = ask.EastM;

  const double ring[kCorners * 2] = {lat - halfDeg,
                                     lon - halfDeg,
                                     lat - halfDeg,
                                     lon + halfDeg,
                                     lat + halfDeg,
                                     lon + halfDeg,
                                     lat + halfDeg,
                                     lon - halfDeg};
  const double corners[kCorners] = {0.0, 0.0, 0.0, 0.0};
  double anchor[3] = {0.0, 0.0, 0.0};

  StructurePlan plan;
  plan.RingLatLon = Span<const double>(ring, kCorners * 2);
  plan.CornerAslM = Span<const double>(corners, kCorners);
  plan.BaseAslM = 0.0;
  plan.HeightM = 6.0 + 12.0 * Spun(ask.Seed, 1u);
  plan.HeightMeasured = false;
  plan.AnchorEcef = anchor;

  const BuildingMesh mesher;
  Raised raised;
  mesher.Mesh(plan, raised);
  std::vector<float> soup;
  const auto spread = [&soup](const std::vector<float> &corners, const std::vector<uint32_t> &run) {
    for (const uint32_t corner : run) {
      soup.insert(soup.end(),
                  corners.begin() + (std::ptrdiff_t)corner * 8,
                  corners.begin() + (std::ptrdiff_t)corner * 8 + 8);
    }
  };
  spread(raised.WallCorners, raised.WallRun);
  spread(raised.RoofCorners, raised.RoofRun);
  if (soup.empty()) { return false; }

  Meshed made;
  if (!made.Take("structure", MaterialInstance(0), soup.data(), soup.size())) { return false; }
  Geometry stood = made.Handed();
  if (stood.parts() == 0) { return false; }

  Material walls;
  walls.BaseColour[0] = 0.62f;
  walls.BaseColour[1] = 0.60f;
  walls.BaseColour[2] = 0.56f;
  walls.Roughness = 0.85f;
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

} // namespace outshine::Generators
