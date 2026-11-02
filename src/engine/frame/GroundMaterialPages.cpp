#include "EngineState.h"
#include "LitterPageStream.h"
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine {
Result Engine::State::AdvanceGroundMaterialPages() {
  auto &renderer = Picture.Device;
  if (!Picture.Standing || renderer.GroundFieldSamples() == 0 || World.SurfaceOwner == nullptr ||
      !World.Surface().MaterialAssets()) {
    return {};
  }
  const size_t count = renderer.GroundFieldSamples();
  if (!World.MaterialPages) {
    const auto pipeline = Services.AssetPublications();
    auto *atlas = renderer.GroundFields();
    if (!pipeline || atlas == nullptr) { return {}; }
    auto stream = LitterPageStream::Create(*pipeline,
                                           World.Surface().MaterialAssets(),
                                           *atlas,
                                           {.DemandMost = count, .PendingMost = 32});
    if (!stream) {
      if (stream.error().Kind == Render::Native::ErrorKind::Busy) { return {}; }
      return std::unexpected(stream.error().Message);
    }
    World.MaterialDemand.resize(count);
    World.MaterialPages = std::move(*stream);
  }
  const auto feedback = renderer.TakeGroundFieldRequests(World.MaterialDemand);
  if (!feedback) { return std::unexpected(feedback.error().Message); }
  if (*feedback) {
    const auto observed =
        World.MaterialPages->Observe(std::span(World.MaterialDemand).first(**feedback));
    if (!observed) { return std::unexpected(observed.error().Message); }
  }
  const auto advanced = World.MaterialPages->Advance();
  if (!advanced) { return std::unexpected(advanced.error().Message); }
  return {};
}
}
