#ifndef OUTSHINE_TEST_ENGINE_NATIVEBUILDINGREPLAY_H
#define OUTSHINE_TEST_ENGINE_NATIVEBUILDINGREPLAY_H
#include "Check.h"
#include "SurfacePreparation.h"
#include "MvtWireFixture.h"
#include <chrono>
#include <cstddef>
#include <expected>
#include <cstdint>
#include <string>

namespace outshine::Test::BuildingReplay {

inline outshine::Test::Mvt::Bytes BuildingTile(bool twoCells) {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 9, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', 's', 0x78, 2, 0x28, 64};
  Append(layer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
  Append(layer, 0x22, Bytes{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g'});
  Bytes feature{0x08, 1, 0x18, 3};
  Append(feature, 0x12, Bytes{0, 0});
  Append(feature, 0x22, Bytes{9, 62, 62, 26, 8, 0, 0, 8, 7, 0, 15});
  Append(layer, 0x12, feature);
  if (twoCells) {
    Bytes second{0x08, 3, 0x18, 3};
    Append(second, 0x12, Bytes{0, 0});
    Append(second, 0x22, Bytes{9, 20, 20, 26, 8, 0, 0, 8, 7, 0, 15});
    Append(layer, 0x12, second);
  }
  Bytes tile;
  Append(tile, 0x1a, layer);
  Bytes streetLayer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 64};
  Append(streetLayer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
  Append(streetLayer, 0x22, Bytes{0x0a, 11, 'r', 'e', 's', 'i', 'd', 'e', 'n', 't', 'i', 'a', 'l'});
  Bytes street{0x08, 2, 0x18, 2};
  Append(street, 0x12, Bytes{0, 0});
  Append(street, 0x22, Bytes{9, 0, 0, 10, 120, 0});
  Append(streetLayer, 0x12, street);
  Append(tile, 0x1a, streetLayer);
  return tile;
}

class ReplaySourceTransport final : public outshine::Data::SourceTransport {
public:
  explicit ReplaySourceTransport(bool twoCells = false) : Payload_(BuildingTile(twoCells)) {}

  outshine::Data::FetchStart Begin([[maybe_unused]] const std::string &url) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::FetchStart Begin(const std::string &url, size_t bodyBytesMost) override {
    if (Payload_.size() > bodyBytesMost) {
      return std::unexpected(outshine::Data::SourceFailureReason::CapacityRefused);
    }
    return Begin(url);
  }

  outshine::Data::Wire Collect([[maybe_unused]] outshine::Data::Ticket ticket) override {
    return outshine::Data::Wire::Answered(200, Payload_);
  }

  void Cancel([[maybe_unused]] outshine::Data::Ticket ticket) override {}

private:
  // Fixed source inputs exist before requests; admission precedes copying a response.
  const Mvt::Bytes Payload_;
};

using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

inline bool PrepareReplayInputs(SurfacePreparation &stack,
                                LongitudeLatitude eye,
                                bool warm,
                                std::chrono::steady_clock::time_point deadline) {
  const auto inputsReady = [&] {
    return warm ? stack.Vectors() != nullptr && stack.Vectors()->SettledWithin(0)
                : stack.Ingested();
  };
  while (!inputsReady() && std::chrono::steady_clock::now() < deadline) {
    CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = warm ? 0u : 1u, .VectorRing = 0}).has_value(),
          "the real vector snapshot and semantic inputs advance");
    (void)stack.AwaitProgress(0.001);
  }
  CHECK(inputsReady() && stack.Vectors() != nullptr && stack.Vectors()->Tiles().size() == 1,
        "one complete vector tile supplies the native product");
  if (!inputsReady() || stack.Vectors() == nullptr || stack.Vectors()->Tiles().size() != 1) {
    return false;
  }
  CHECK(inputsReady(), "the required source or native admission metadata is available");
  CHECK(!warm || stack.Ways().LookedCount() == 0,
        "warm replay starts without preparing street inputs");
  CHECK(warm || !stack.Ways().Ways().empty(),
        "the cold asset binds an actual street corridor instead of an empty digest");
  CHECK(!warm || (stack.PreparedVectorCosts().Hits > 0 && stack.PreparedVectorCosts().Writes == 0 &&
                  stack.Vectors()->TotalBuildMetrics().ParseMs == 0),
        "fresh warm replay loads normalized native tiles before source decode");
  return true;
}

}
#endif
