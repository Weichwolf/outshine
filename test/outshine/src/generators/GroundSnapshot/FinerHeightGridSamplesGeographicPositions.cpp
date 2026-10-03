#include "GroundSnapshot.h"
#include "GroundPatch.h"
#include "Check.h"
#include <cmath>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

class Heights final : public GroundQuery {
public:
  explicit Heights(const Tile &region) : Region(region) {}

  const Tile &Region;
  mutable std::vector<LongitudeLatitude> Requested;
  mutable size_t Blocks = 0;
  GroundSample::State State = GroundSample::State::Resolved;

  GroundSample At(LongitudeLatitude at) const override {
    Requested.push_back(at);
    if (State == GroundSample::State::Pending) { return GroundSample::Waiting(); }
    if (State == GroundSample::State::Hole) { return GroundSample::Missing(); }
    return GroundSample::At(at.LatitudeDeg + at.LongitudeDeg);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override {
    ++Blocks;
    return {};
  }

  double PostM(double) const override { return Region.SpanNm() / 4; }

  int BlockZoom() const override { return Region.Zoom() + 2; }
};
}

int main() {
  using namespace outshine::Test;
  const auto region = Tile::Of(14, {.LongitudeDeg = 16.3738, .LatitudeDeg = 48.2082});
  Heights heights(region);
  Snapped status = Snapped::Taken;
  const auto patch = PatchOver(region, heights, &status);
  CHECK(patch && status == Snapped::Taken,
        "coarse generator region resolves over a finer height grid");
  CHECK(heights.Blocks == 0 && heights.Requested.size() == 25,
        "coarse tile indices are never reused in the finer address space");
  if (patch && heights.Requested.size() == 25) {
    for (int row = 0; row < 5; ++row) {
      for (int column = 0; column < 5; ++column) {
        const EastNorth at{.EastM = column * region.SpanEm() / 4,
                           .NorthM = row * region.SpanNm() / 4};
        const auto expected = region.Geo(at);
        const auto actual = heights.Requested[static_cast<size_t>(row * 5 + column)];
        CHECK(actual == expected && std::abs(patch->HeightAslM(at) - expected.LatitudeDeg -
                                             expected.LongitudeDeg) < 1e-9,
              "every posting retains its geographic position and analytic height");
      }
    }
  }
  heights.State = GroundSample::State::Pending;
  status = Snapped::Taken;
  CHECK(!PatchOver(region, heights, &status) && status == Snapped::Waiting,
        "pending source yields waiting without a partial patch");
  heights.State = GroundSample::State::Hole;
  status = Snapped::Taken;
  CHECK(!PatchOver(region, heights, &status) && status == Snapped::NoGround,
        "missing source yields no ground without a fabricated floor");
  return Report();
}
