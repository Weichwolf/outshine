#include "AssetCache.h"
#include "BuildingSurface.h"
#include "PreparedStructureCodec.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>

int main(int argc, const char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc != 2) { return 2; }
  const auto cache = AssetCache::Open(argv[1]);
  if (!cache) { return 3; }
  std::string form, basisKey, current;
  std::set<std::array<int64_t, 2>> seen;
  Vec3 anchor{};
  bool qualified = false;
  size_t sources = 0;
  while (std::cin >> basisKey >> form) {
    if (basisKey != current) {
      const auto loaded = (*cache)->Load(basisKey, 64u << 20);
      if (!loaded || !*loaded) { return 4; }
      const auto basis = DecodePreparedBuildingBasis((**loaded).Bytes(), 128u << 20);
      if (!basis) { return 5; }
      anchor = basis->AnchorEcef;
      current = basisKey;
      qualified = basis->HeightQualified;
    }
    if (!qualified) { continue; }
    const auto loaded = (*cache)->Load(form, 64u << 20);
    if (!loaded || !*loaded) { return 6; }
    const auto block = DecodeBuildingSurfaceBlock((**loaded).Bytes(), 128u << 20);
    if (!block) { return 7; }
    for (const auto &building : *block) {
      const auto shapes = building.Shapes();
      if (shapes.empty()) { continue; }
      ++sources;
      if (shapes.size() != 1) { continue; }
      const auto &s = shapes.front();
      if (s.Ring.size() != 4 || !s.OnGround() || !s.Holes.empty() || s.RoundFootprint ||
          (s.Roof != RoofKind::Flat && s.Roof != RoofKind::Gable && s.Roof != RoofKind::Shed)) {
        continue;
      }
      const auto &axes = building.Axes();
      const auto origin = building.Origin() + anchor;
      const auto position =
          GeoFromEcef(origin + axes.East * s.Centre.EastM + axes.North * s.Centre.NorthM);
      if (!position ||
          !seen.emplace(std::array<int64_t, 2>{std::llround(position->LongitudeDeg * 1.e7),
                                               std::llround(position->LatitudeDeg * 1.e7)})
               .second) {
        continue;
      }
      std::array<double, 29> record{};
      record[0] = static_cast<double>(s.Roof);
      size_t at = 1;
      for (const Vec3 &axis : {origin, axes.East, axes.North, axes.Up}) {
        for (size_t i = 0; i < 3; ++i) { record[at++] = axis[i]; }
      }
      for (const auto &p : s.Ring) {
        record[at++] = p.EastM;
        record[at++] = p.NorthM;
      }
      const double eaves = s.SeatM + s.FootM + s.EavesM;
      for (const double p : {s.SoleM,
                             eaves,
                             s.RiseM,
                             s.HalfVm,
                             s.Centre.EastM,
                             s.Centre.NorthM,
                             s.AxisV().EastM,
                             s.AxisV().NorthM}) {
        record[at++] = p;
      }
      std::cout.write(reinterpret_cast<const char *>(record.data()), sizeof(record));
    }
  }
  std::cerr << "Inspected " << sources << " prepared surfaces, exported " << seen.size()
            << " approximate unique centres\n";
  return std::cin.eof() && std::cout ? 0 : 8;
}
