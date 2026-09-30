#include "TerrainLoader.h"
#include "SourceSet.h"
#include "ContentStore.h"
#include <world/data/Transport.h>
#include "Check.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace outshine;

namespace {
constexpr std::array<uint8_t, 81> kGradientPng{
    137, 80, 78,  71, 13, 10,  26, 10,  0,   0,   0,  13,  73, 72,  68,  82, 0,   0,  0,  4,   0,
    0,   0,  4,   8,  2,  0,   0,  0,   38,  147, 9,  41,  0,  0,   0,   24, 73,  68, 65, 84,  120,
    156, 99, 104, 72, 97, 104, 72, 101, 104, 72,  99, 104, 72, 103, 96,  32, 142, 3,  0,  124, 247,
    14,  89, 156, 77, 91, 71,  0,  0,   0,   0,   73, 69,  78, 68,  174, 66, 96,  130};

class GradientSource final : public Data::Source {
public:
  Data::SourceDecl Decl{
      .Id = "longitude-gradient", .Revision = "analytic-v1", .Keeps = Data::Cacheability::Never};

  const Data::SourceDecl &Declaration() const noexcept override { return Decl; }

  Data::Coverage Covers(const Data::Fetch &) const noexcept override {
    return Data::Coverage::Inside;
  }

  Data::Address Serves(const Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  Data::FetchStart Begin(const Data::Address &, Data::Transport &) const override {
    return Data::Ticket::None;
  }

  Data::Fetched Collect(const Data::Address &, Data::Ticket, Data::Transport &) const override {
    return Data::Fetched::Delivered(std::vector<uint8_t>(kGradientPng.begin(), kGradientPng.end()));
  }
};

class NoNetwork final : public Data::Transport {
public:
  Data::FetchStart Begin(const std::string &) override { return Data::Ticket::None; }

  Data::Wire Collect(Data::Ticket) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket) override {}
};

bool HasHeight(const GroundSample &sample, double expected) {
  return sample.AslM() && std::abs(*sample.AslM() - expected) < 1e-5;
}
}

int main() {
  using namespace outshine::Test;
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<GradientSource>()) == Data::SourceSet::Registration::Accepted,
        "the independently encoded four-column DEM is registered");
  NoNetwork network;
  Ground::TilePool pool({.PollAttempts = 1000}, sources, network);
  Ground::GroundStream ground(pool, {.Z = 6, .Grid = 4});
  const LongitudeLatitude position{.LongitudeDeg = -179.0, .LatitudeDeg = 0.0};
  auto warm = ground.At(position);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (warm.Where() == GroundSample::State::Pending &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    warm = ground.At(position);
  }
  CHECK(warm.Where() == GroundSample::State::Resolved, "the sourced gradient resolves in time");
  if (warm.Where() != GroundSample::State::Resolved) { return Report(); }
  // PNG columns are 100, 101, 102, 103 metres. In this first cell, h = 100 + 3u.
  // At zoom six, -179 degrees is u = 64/360; +/-180 is u = 0 in the same field.
  for (const double lon : {-179.0, 181.0, -539.0, 541.0}) {
    const double expected = 100.0 + 3.0 * 64.0 / 360.0;
    CHECK(HasHeight(ground.At({.LongitudeDeg = lon, .LatitudeDeg = 0.0}), expected),
          "querying a wrapped longitude matches the independent DEM column fraction");
    const auto resident = ground.Resident({.LongitudeDeg = lon, .LatitudeDeg = 0.0});
    CHECK(HasHeight(resident, expected) && resident.CoarseBy() == 0,
          "resident interpolation uses the same geographic point without degrading its source");
    const auto &normal = resident.NormalM();
    const double length2 = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
    CHECK(std::abs(length2 - 1.0) < 1e-10 && normal[0] < 0.0 && normal[1] > 0.0 &&
              std::abs(normal[2]) < 1e-10,
          "the sourced eastward gradient has a unit upward normal facing west");
  }
  for (const double lon : {-180.0, 180.0, -540.0, 540.0}) {
    CHECK(HasHeight(ground.At({.LongitudeDeg = lon, .LatitudeDeg = 0.0}), 100.0) &&
              HasHeight(ground.Resident({.LongitudeDeg = lon, .LatitudeDeg = 0.0}), 100.0),
          "antimeridian endpoints use the first sample rather than the far edge of the tile");
  }
  for (const double bad : {std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    for (const auto at : {LongitudeLatitude{.LongitudeDeg = bad, .LatitudeDeg = 0.0},
                          LongitudeLatitude{.LongitudeDeg = 0.0, .LatitudeDeg = bad}}) {
      CHECK(ground.At(at).Where() == GroundSample::State::Hole &&
                ground.Resident(at).Where() == GroundSample::State::Hole,
            "nonfinite geographic input is refused before normalization or integer casts");
    }
  }
  return Report();
}
