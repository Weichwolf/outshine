#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "BuildingMesh.h"
#include "TileGeodesy.h"
#include "BuildingShape.h"
#include "RoofSurface.h"
#include "Check.h"
#include "StructureMesher.h"

// WHAT MUST BE TRUE OF A BUILDING THE GENERATOR MAKES, stated once and then asserted for every
// architecture it chooses.
//
// Unreal refuses a non-manifold cluster boundary in Nanite outright; RAGE's shells are authored and
// a hole is caught in review. Both treat closedness as a PROPERTY OF THE ASSET rather than something
// the renderer copes with, so the matter is closed. What neither can give this tree is the check
// itself, because their buildings are drawn by hand and these are GROWN -- so the check has to be a
// walk over the triangles, run for every case the grower can reach.
//
// FOUR THINGS MUST BE TRUE, and each is a property of a SOLID rather than of a picture:
//
//   CLOSED       every edge lies on exactly two triangles. An edge on one is a HOLE: you can see
//                inside the building. An edge on three is not a surface at all
//   ORIENTED     the two triangles sharing an edge traverse it in OPPOSITE directions. This is what
//                makes "outside" well defined; without it a shell can be closed and still be inside
//                out in places, which is the defect that reads as black facades
//   SOLID        the signed volume, by the divergence theorem over the closed shell, is POSITIVE.
//                A shell that is closed and oriented but encloses negative volume is inside out
//                ENTIRELY, which the per-edge test cannot see because it is locally consistent
//   HONEST       no triangle has two corners in one place. A degenerate triangle has no normal, so
//                it shades as noise and hides a hole from the edge count by pairing with itself
//
// POSITIONS ARE WELDED ON A CENTIMETRE GRID before edges are gathered. The soup is unindexed -- the
// same corner appears once per triangle that touches it -- so an unwelded walk would call every
// shared edge a hole. A centimetre is the tolerance the owner set for infrastructure and it means
// the same thing here: closer than that, two corners are one corner.
//
// WHAT THIS CASE DOES NOT COVER, on its own page. It cannot see OVERLAP: two parts of one building
// interpenetrating are both still closed, oriented and positive, and nothing here notices. It says
// nothing about whether the building is the RIGHT shape for its tags -- only that whatever shape it
// chose is a solid. It reaches the architectures its footprints happen to elicit and PRINTS which,
// so a case the grower can make and this suite never sees is visible as an absence rather than
// mistaken for a pass. And it says nothing about where the building STANDS: seating on the ground
// is board:2028's and is a different oracle.

namespace {

using namespace outshine;
using namespace outshine::Generators;

constexpr size_t kSoupStride = 8;

[[nodiscard]] const char *Named(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return "outbuilding";
    case BuildingUse::House: return "house";
    case BuildingUse::Terrace: return "terrace";
    case BuildingUse::Block: return "block";
    case BuildingUse::Hall: return "hall";
    case BuildingUse::Tower: return "tower";
    case BuildingUse::Spire: return "spire";
  }
  return "?";
}

[[nodiscard]] const char *Named(RoofKind roof) {
  switch (roof) {
    case RoofKind::Flat: return "flat";
    case RoofKind::Gable: return "gable";
    case RoofKind::Hip: return "hip";
    case RoofKind::Shed: return "shed";
    case RoofKind::Mansard: return "mansard";
    case RoofKind::Sawtooth: return "sawtooth";
    case RoofKind::Dome: return "dome";
  }
  return "?";
}

struct Verdict {
  size_t Triangles = 0;
  size_t Degenerate = 0;
  size_t Holes = 0;
  size_t Overused = 0;
  size_t Reversed = 0;
  double VolumeM3 = 0.0;
  double HoleLowZ = 0.0, HoleHighZ = 0.0;
  double FlipLowZ = 0.0, FlipHighZ = 0.0;
  double BaseZ = 0.0, TopZ = 0.0;

  [[nodiscard]] bool Whole(void) const {
    return Triangles > 0 && Degenerate == 0 && Holes == 0 && Overused == 0 && Reversed == 0 &&
           VolumeM3 > 0.0;
  }
};

[[nodiscard]] Verdict Judge(const std::vector<float> &soup, const double up[3]) {
  Verdict out;
  const size_t corners = soup.size() / kSoupStride;
  std::unordered_map<uint64_t, uint32_t> seenAt;
  std::vector<uint32_t> welded;
  welded.reserve(corners);
  std::vector<double> at;
  at.reserve(corners * 3);
  for (size_t one = 0; one < corners; ++one) {
    const double x = (double)soup[one * kSoupStride];
    const double y = (double)soup[one * kSoupStride + 1];
    const double z = (double)soup[one * kSoupStride + 2];
    const int64_t cx = (int64_t)std::llround(x * 100.0);
    const int64_t cy = (int64_t)std::llround(y * 100.0);
    const int64_t cz = (int64_t)std::llround(z * 100.0);
    const uint64_t key =
        (uint64_t)(cx * 73856093LL) ^ (uint64_t)(cy * 19349663LL) ^ (uint64_t)(cz * 83492791LL);
    const auto found = seenAt.find(key);
    if (found != seenAt.end()) {
      welded.push_back(found->second);
      continue;
    }
    const uint32_t made = (uint32_t)(at.size() / 3);
    seenAt.emplace(key, made);
    at.push_back(x);
    at.push_back(y);
    at.push_back(z);
    welded.push_back(made);
  }

  for (size_t one = 0; one * 3 + 2 < at.size(); ++one) {
    const double z = at[one * 3] * up[0] + at[one * 3 + 1] * up[1] + at[one * 3 + 2] * up[2];
    if (one == 0) { out.BaseZ = out.TopZ = z; continue; }
    if (z < out.BaseZ) { out.BaseZ = z; }
    if (z > out.TopZ) { out.TopZ = z; }
  }
  std::map<std::pair<uint32_t, uint32_t>, int> facing;
  for (size_t tri = 0; tri + 2 < welded.size(); tri += 3) {
    const uint32_t corner[3] = {welded[tri], welded[tri + 1], welded[tri + 2]};
    if (corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0]) {
      ++out.Degenerate;
      continue;
    }
    ++out.Triangles;
    double signedSix = 0.0;
    const double *a = &at[(size_t)corner[0] * 3];
    const double *b = &at[(size_t)corner[1] * 3];
    const double *c = &at[(size_t)corner[2] * 3];
    signedSix = a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                a[2] * (b[0] * c[1] - b[1] * c[0]);
    out.VolumeM3 += signedSix / 6.0;
    for (int side = 0; side < 3; ++side) {
      const uint32_t from = corner[side];
      const uint32_t to = corner[(side + 1) % 3];
      const bool ahead = from < to;
      const std::pair<uint32_t, uint32_t> key{ahead ? from : to, ahead ? to : from};
      facing[key] += ahead ? 1 : -1;
    }
  }
  bool anyHole = false;
  for (const auto &edge : facing) {
    const int walked = std::abs(edge.second);
    if (walked != 1) { continue; }
    ++out.Holes;
    const double *pa = &at[(size_t)edge.first.first * 3];
    const double *pb = &at[(size_t)edge.first.second * 3];
    const double za = pa[0] * up[0] + pa[1] * up[1] + pa[2] * up[2];
    const double zb = pb[0] * up[0] + pb[1] * up[1] + pb[2] * up[2];
    const double low = za < zb ? za : zb;
    const double high = za < zb ? zb : za;
    if (!anyHole) {
      out.HoleLowZ = low;
      out.HoleHighZ = high;
      anyHole = true;
      continue;
    }
    if (low < out.HoleLowZ) { out.HoleLowZ = low; }
    if (high > out.HoleHighZ) { out.HoleHighZ = high; }
  }
  std::map<std::pair<uint32_t, uint32_t>, int> counted;
  for (size_t tri = 0; tri + 2 < welded.size(); tri += 3) {
    const uint32_t corner[3] = {welded[tri], welded[tri + 1], welded[tri + 2]};
    if (corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0]) { continue; }
    for (int side = 0; side < 3; ++side) {
      const uint32_t from = corner[side];
      const uint32_t to = corner[(side + 1) % 3];
      const bool ahead = from < to;
      counted[{ahead ? from : to, ahead ? to : from}] += 1;
    }
  }
  for (const auto &edge : counted) {
    if (edge.second > 2) { ++out.Overused; }
  }
  bool anyFlip = false;
  for (const auto &edge : facing) {
    if (counted[edge.first] != 2 || edge.second == 0) { continue; }
    ++out.Reversed;
    const double *pa = &at[(size_t)edge.first.first * 3];
    const double *pb = &at[(size_t)edge.first.second * 3];
    const double za = pa[0] * up[0] + pa[1] * up[1] + pa[2] * up[2];
    const double zb = pb[0] * up[0] + pb[1] * up[1] + pb[2] * up[2];
    const double low = za < zb ? za : zb, high = za < zb ? zb : za;
    if (!anyFlip) { out.FlipLowZ = low; out.FlipHighZ = high; anyFlip = true; continue; }
    if (low < out.FlipLowZ) { out.FlipLowZ = low; }
    if (high > out.FlipHighZ) { out.FlipHighZ = high; }
  }
  return out;
}

struct Footprint {
  const char *What;
  std::vector<double> RingLatLon;
  double HeightM;
};

[[nodiscard]] std::vector<double> Box(double lat, double lon, double northM, double eastM) {
  const double perLat = 1.0 / 111132.0;
  const double perLon = 1.0 / (111320.0 * std::cos(lat * 3.14159265358979 / 180.0));
  const double n = northM * 0.5 * perLat;
  const double e = eastM * 0.5 * perLon;
  return {lat - n, lon - e, lat - n, lon + e, lat + n, lon + e, lat + n, lon - e};
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double lat = 49.3777, lon = 10.179;
  const std::vector<Footprint> asked = {
      {"a shed, 4 by 3 m", Box(lat, lon, 4.0, 3.0), 2.6},
      {"a house, 12 by 9 m", Box(lat, lon, 12.0, 9.0), 7.5},
      {"a terrace, 40 by 8 m", Box(lat, lon, 40.0, 8.0), 9.0},
      {"a block, 30 by 24 m", Box(lat, lon, 30.0, 24.0), 18.0},
      {"a hall, 70 by 45 m", Box(lat, lon, 70.0, 45.0), 11.0},
      {"a tower, 11 by 11 m", Box(lat, lon, 11.0, 11.0), 42.0},
      {"a spire, 6 by 6 m", Box(lat, lon, 6.0, 6.0), 55.0},
      {"a long hall, 120 by 30 m", Box(lat, lon, 120.0, 30.0), 14.0},
      {"a slab, 60 by 16 m", Box(lat, lon, 60.0, 16.0), 26.0},
  };

  BuildingMesh grows;
  std::map<std::string, size_t> reached;
  size_t whole = 0, closed = 0;
  size_t holesInAll = 0, reversedInAll = 0, degenerateInAll = 0, negative = 0;

  for (const Footprint &one : asked) {
    Frontage street;
    const Massing massed =
        MassOf(Span<const double>(one.RingLatLon.data(), one.RingLatLon.size()), one.HeightM, true,
               street);
    std::string architecture;
    double halfU = 0.0, halfV = 0.0, overhang = 0.0, rise = 0.0;
    for (const BuildingShape &part : massed.Parts) {
      halfU = part.HalfUm; halfV = part.HalfVm; overhang = part.FootM + part.EavesM;
      rise = part.RiseM;
      if (!architecture.empty()) { architecture += " + "; }
      architecture += std::string(Named(part.Use)) + "/" + Named(part.Roof);
      reached[std::string(Named(part.Use)) + "/" + Named(part.Roof)] += 1;
    }

    const outshine::Ground::Ecef anchor =
        outshine::Ground::GeoToEcefWgs84(outshine::Ground::Geo{lon, lat, 0.0});
    const double anchorEcef[3] = {anchor.X, anchor.Y, anchor.Z};
    StructurePlan plan;
    plan.AnchorEcef = anchorEcef;
    plan.RingLatLon = Span<const double>(one.RingLatLon.data(), one.RingLatLon.size());
    plan.BaseAslM = 0.0;
    plan.HeightM = one.HeightM;
    plan.HeightMeasured = true;
    plan.Street = street;
    (void)RoofSurface::UnclippedTaken();
    (void)RoofSurface::OutsideTaken();
    std::vector<float> soup;
    grows.Mesh(plan, soup);
    (void)RoofSurface::UnclippedTaken();
    (void)RoofSurface::OutsideTaken();

    const double reach = std::sqrt(anchorEcef[0] * anchorEcef[0] + anchorEcef[1] * anchorEcef[1] +
                                   anchorEcef[2] * anchorEcef[2]);
    const double up[3] = {anchorEcef[0] / reach, anchorEcef[1] / reach, anchorEcef[2] / reach};
    const Verdict said = Judge(soup, up);
    holesInAll += said.Holes;
    reversedInAll += said.Reversed;
    degenerateInAll += said.Degenerate;
    if (said.VolumeM3 <= 0.0) { ++negative; }
    if (said.Whole()) { ++whole; }
    if (said.Holes == 0 && said.Overused == 0 && said.Degenerate == 0) { ++closed; }
    std::printf("%-22s %-18s %5zu tri %4zu hole %4zu over %4zu deg  halfU %6.2f halfV %6.2f d %+8.5f over %4.2f rise %6.2f  holes at %6.2f..%6.2f\n",
                one.What, architecture.c_str(), said.Triangles, said.Holes, said.Overused,
                said.Degenerate, halfU, halfV, halfU - halfV, overhang, rise,
                said.HoleLowZ - said.BaseZ, said.HoleHighZ - said.BaseZ);
  }

  std::printf("\n%zu of %zu buildings are WHOLE\n", whole, asked.size());
  std::printf("ARCHITECTURES REACHED (%zu distinct):", reached.size());
  for (const auto &one : reached) { std::printf(" %s x%zu", one.first.c_str(), one.second); }
  std::printf("\n");

  CHECK(reached.size() >= 4,
        "**THE CONTROL: THE SWEEP REACHES SEVERAL ARCHITECTURES**. Every claim below is about the "
        "grower's output, and a sweep that elicited one shape would prove nothing about the rest. "
        "The distinct pairs are printed above so a case the grower can make and this suite never "
        "sees is an absence a reader can see, rather than a pass");

  CHECK(degenerateInAll == 0,
        "**NO TRIANGLE HAS TWO CORNERS IN ONE PLACE**: a degenerate triangle has no normal, so it "
        "shades as noise -- and it can pair with itself across the edge count and HIDE a hole from "
        "the very test below");

  CHECK(holesInAll == 0,
        "**EVERY BUILDING IS CLOSED**: an edge on one triangle is a hole you can see inside the "
        "building through. Nanite refuses a non-manifold boundary outright and RAGE's shells are "
        "authored closed; a GROWN building has to earn the same property by construction");

  CHECK(reversedInAll == 0,
        "**AND ORIENTED**: the two triangles sharing an edge must walk it in opposite directions. "
        "Without that, a shell can be closed and still inside out in places -- which is the defect "
        "that reads as a black facade beside a lit one");

  std::printf("shells closed enough to have a volume at all: %zu of %zu\n", closed, asked.size());
  CHECK(closed == 0 || negative == 0,
        "**AND ENCLOSES POSITIVE VOLUME**: by the divergence theorem over a closed oriented shell. "
        "A shell can be closed and locally consistent and still be inside out ENTIRELY, which no "
        "per-edge test can see. THIS CHECK IS VACUOUS WHILE NOTHING IS CLOSED and the count above "
        "says so -- the divergence theorem means nothing over an open shell, so the figures in the "
        "table are not volumes yet and must not be read as any");

  Covers("board:2031 -- every architecture the grower makes is a closed, oriented solid");
  return Report();
}
