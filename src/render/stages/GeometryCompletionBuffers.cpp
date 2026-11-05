#include "GeometryRenderer.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {
bool GeometryRenderer::PrepareCompletionBuffers(std::string &error) {
  using S = GeometryResidency::Stream;
  constexpr auto phase = GeometryResidency::SelectionPhase::Completion;
  for (const auto stream : {S::ClusterKept,
                            S::ClusterSelected,
                            S::ClusterSelectedState,
                            S::ClusterSlot,
                            S::DrawIndex,
                            S::DrawArguments}) {
    const auto *source = Bound().Buffer(stream).get();
    if (source == nullptr) {
      error = "primary selection buffer is missing";
      return false;
    }
    const auto &shape = source->Description();
    const auto grown = Bound().Grow(GeometryResidency::SelectionStream(stream, phase),
                                    {.Usage = shape.Use,
                                     .Bytes = static_cast<uint32_t>(shape.Bytes),
                                     .Existing = GeometryResidency::ExistingContents::Discard,
                                     .Deferred = true});
    if (!grown) {
      error = grown.error().Message;
      return false;
    }
  }
  static_assert(sizeof(std::array<uint32_t, 2>) == 8);
  std::vector<std::array<uint32_t, 2>> accepted(Jobs_);
  std::vector<uint32_t> slots(Jobs_, 0xffffffffu);
  slots.resize(slots.size() + Batches.size(), kGeometryIndexCold);
  const auto storage = Native::BufferUse::Storage;
  std::array crossings{
      GeometryResidency::Crossing{
          .Which = S::PredictionAccepted,
          .Usage = storage,
          .From = accepted.data(),
          .Bytes = static_cast<uint32_t>(accepted.size() * sizeof(accepted.front()))},
      GeometryResidency::Crossing{.Which = S::CompletionSlot,
                                  .Usage = storage,
                                  .From = slots.data(),
                                  .Bytes = static_cast<uint32_t>(slots.size() * sizeof(uint32_t))},
      GeometryResidency::Crossing{.Which = S::CompletionDrawArguments,
                                  .Usage = storage | Native::BufferUse::Indirect,
                                  .From = Args_.data(),
                                  .Bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t))}};
  const auto crossed = Bound().Cross(crossings, true);
  if (!crossed) {
    error = crossed.error().Message;
    return false;
  }
  return true;
}
}
