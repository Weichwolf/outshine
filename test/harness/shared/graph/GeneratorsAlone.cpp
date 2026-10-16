#include <span>
#include <cstddef>
#include <print>
#include <vector>

#include "GroundPatch.h"
#include "GroundTable.h"
#include "Tile.h"
#include <Earth.h>

int main() {
  using namespace outshine::Generators;
  constexpr float kFrictionBase = 0.3f;
  constexpr float kFrictionStep = 0.1f;
  constexpr double kGroundHeightAslM = 500;
  std::vector<GroundTable::Row> rows(3);
  for (size_t at = 0; at < rows.size(); ++at) {
    rows[at].FrictionMu = kFrictionBase + kFrictionStep * static_cast<float>(at);
  }
  const auto table = GroundTable::Of(std::span<const GroundTable::Row>(rows.data(), rows.size()));

  const Tile region =
      Tile::Of(14, outshine::LongitudeLatitude{.LongitudeDeg = 11.5756, .LatitudeDeg = 48.1372});
  const int side = 9;
  std::vector<GroundPatch::Posting> postings(static_cast<size_t>(side) * static_cast<size_t>(side));
  for (auto &one : postings) { one.Height = outshine::GroundSample::At(kGroundHeightAslM); }
  const auto patch = GroundPatch::Complete(
      region, side, std::span<const GroundPatch::Posting>(postings.data(), postings.size()));

  std::println("table {}  patch {}  height {:.3f}",
               table ? "built" : "null",
               patch ? "built" : "null",
               patch ? patch->HeightAslM(outshine::EastNorth{.EastM = 0.0, .NorthM = 0.0}) : -1.0);
  return table && patch ? 0 : 1;
}
