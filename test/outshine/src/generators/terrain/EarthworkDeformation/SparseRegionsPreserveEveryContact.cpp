#include "Check.h"
#include "EarthworkDeformation.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {
outshine::EarthworkStamp Pad(outshine::EastNorthUp position) {
  const double eastM = position.EastM;
  const double northM = position.NorthM;
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {
      eastM, northM, eastM + 4, northM, eastM + 4, northM + 4, eastM, northM + 4};
  stamp.LowE = stamp.AtE = eastM;
  stamp.LowN = stamp.AtN = northM;
  stamp.HighE = eastM + 4;
  stamp.HighN = northM + 4;
  stamp.PlateauM = position.UpM;
  stamp.Fills = true;
  return stamp;
}

auto Claims(std::span<const outshine::EarthworkPointClaim> claims) {
  std::vector<std::pair<uint32_t, uint32_t>> result;
  for (const auto claim : claims) { result.emplace_back(claim.Point, claim.Stamp); }
  std::ranges::sort(result);
  return result;
}

void Verify(double offsetM, bool reverse) {
  using namespace outshine;
  using namespace outshine::Test;
  std::array stamps{Pad({.EastM = 0, .NorthM = 0, .UpM = 2}),
                    Pad({.EastM = 2, .NorthM = 0, .UpM = 2}),
                    Pad({.EastM = offsetM, .NorthM = offsetM, .UpM = 4}),
                    Pad({.EastM = offsetM + 2, .NorthM = offsetM, .UpM = 4})};
  if (reverse) { std::ranges::reverse(stamps); }
  const std::array points{EastNorth{.EastM = 1, .NorthM = 1},
                          EastNorth{.EastM = 3, .NorthM = 1},
                          EastNorth{.EastM = 5, .NorthM = 1},
                          EastNorth{.EastM = offsetM + 1, .NorthM = offsetM + 1},
                          EastNorth{.EastM = offsetM + 3, .NorthM = offsetM + 1},
                          EastNorth{.EastM = offsetM + 5, .NorthM = offsetM + 1},
                          EastNorth{.EastM = offsetM * 0.5, .NorthM = offsetM * 0.5}};
  std::vector<double> expected(points.size());
  std::vector<uint32_t> owners(points.size(), kNoStamp);
  std::vector<EarthworkPointClaim> claims;
  for (size_t first = 0; first < stamps.size(); first += 2) {
    const auto local = ApplyEarthworkStamps(
        std::span(stamps).subspan(first, 2), points, expected, kMostEarthworkM);
    CHECK(local.Moved == 3 && local.Structures == 0 && local.Held == 0,
          "each independently indexed local group has three valid contacts");
    for (size_t point = 0; point < points.size(); ++point) {
      if (local.DecidedBy[point] != kNoStamp) {
        owners[point] = local.DecidedBy[point] + static_cast<uint32_t>(first);
      }
    }
    for (const auto claim : local.Inside) {
      claims.push_back({.Point = claim.Point, .Stamp = claim.Stamp + static_cast<uint32_t>(first)});
    }
  }
  std::vector<double> heights(points.size());
  EarthworkDeformationJob job(stamps, points, heights, kMostEarthworkM);
  CHECK(!job.Advance(0), "zero work leaves the sparse-region job pending");
  bool done = false;
  for (size_t step = 0; step <= 3 * points.size() && !done; ++step) { done = job.Advance(1); }
  CHECK(done, "one-point slices complete in the existing three phases");
  if (!done) { return; }
  const auto result = job.Take();
  CHECK(heights == expected, "combining distant groups preserves all local contact heights");
  CHECK(result.DecidedBy == owners, "shared-cell ties preserve the original stamp order");
  CHECK(Claims(result.Inside) == Claims(claims), "sparse selection retains every inside claim");
  CHECK(result.Moved == 6 && result.Structures == 0 && result.Held == 0,
        "only the six covered nodes move and no valid source becomes a structure");
  CHECK(result.Refused == std::vector<uint8_t>(stamps.size()),
        "sparse extent does not reject a physically valid contact");
}
}

int main() {
  for (const double offsetM : {-100000.0, 100000.0}) {
    for (const bool reverse : {false, true}) { Verify(offsetM, reverse); }
  }
  return outshine::Test::Report();
}
