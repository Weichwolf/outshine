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

}

bool Structures::Make(const Ask &ask, Geometry &into) const {
  const double sideM = ask.ExtentM > 0.0 && ask.ExtentM < 200.0 ? ask.ExtentM : 12.0;
  const double halfDeg = 0.5 * sideM / kMetresPerDegree;
  const double lat = ask.NorthM;
  const double lon = ask.EastM;

  const double ring[kCorners * 2] = {lat - halfDeg, lon - halfDeg, lat - halfDeg, lon + halfDeg,
                                     lat + halfDeg, lon + halfDeg, lat + halfDeg, lon - halfDeg};
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
  std::vector<float> soup;
  mesher.Mesh(plan, soup);
  if (soup.empty()) { return false; }

  Meshed made;
  if (!made.Take("structure", 0, soup.data(), soup.size())) { return false; }
  Geometry stood = made.Handed();
  if (stood.Parts() == 0) { return false; }

  Material walls;
  walls.BaseColour[0] = 0.62f;
  walls.BaseColour[1] = 0.60f;
  walls.BaseColour[2] = 0.56f;
  walls.Roughness = 0.85f;
  const int named = into.Surface("walls", walls);
  for (int part = 0; part < stood.Parts(); ++part) {
    const int here = into.Part("structure", named);
    if (!into.Positions(here, stood.PositionsOf(part)) ||
        !into.Triangles(here, stood.TrianglesOf(part))) {
      return false;
    }
    if (!stood.NormalsOf(part).empty()) { (void)into.Normals(here, stood.NormalsOf(part)); }
    if (!stood.TextureOf(part).empty()) { (void)into.Texture(here, stood.TextureOf(part)); }
  }
  return true;
}

}
