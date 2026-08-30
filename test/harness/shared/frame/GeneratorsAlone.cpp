#include <cstdio>
#include <vector>

#include "GroundPatch.h"
#include "GroundTable.h"
#include "Tile.h"

int main(void) {
  using namespace outshine::Generators;
  std::vector<GroundTable::Row> rows(3);
  for (size_t at = 0; at < rows.size(); ++at) { rows[at].SlopeMaxDeg = 30.0f + (float)at; }
  const auto table =
      GroundTable::Of(outshine::Span<const GroundTable::Row>(rows.data(), rows.size()));

  const Tile region = Tile::Of(14, 48.1372, 11.5756);
  const int side = 9;
  std::vector<GroundPatch::Posting> postings((size_t)side * (size_t)side);
  for (auto &one : postings) { one.Height = outshine::GroundSample::At(500.0); }
  const auto patch = GroundPatch::Complete(
      region, side, outshine::Span<const GroundPatch::Posting>(postings.data(), postings.size()));

  std::printf("table %s  patch %s  height %.3f\n",
              table ? "built" : "null",
              patch ? "built" : "null",
              patch ? patch->HeightAslM(0.0, 0.0) : -1.0);
  return table && patch ? 0 : 1;
}
