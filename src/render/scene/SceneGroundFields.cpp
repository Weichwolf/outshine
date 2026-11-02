#include "SceneRenderer.h"
#include "FieldPageFeedback.h"
#include "FieldPageFeedbackPass.h"
#include "R8PageAtlas.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void> SceneRenderer::BindGroundFields(FrameResources &frame, bool enabled) {
  if (enabled && !GroundFields_) {
    constexpr R8PageAtlasOptions options;
    const auto storage = R8PageAtlasStorageFor(options);
    if (!storage) { return std::unexpected(storage.error()); }
    auto atlas = R8PageAtlas::Create(*Device_, options, *storage, frame.Handles.Uploads);
    if (!atlas) { return std::unexpected(atlas.error()); }
    GroundFields_ = std::make_unique<R8PageAtlas>(std::move(*atlas));
  }
  ActiveContent().Geometry().GroundFieldsFrom(enabled ? GroundFields_.get() : nullptr);
  return {};
}

Native::Result<void> SceneRenderer::CreateGroundFieldSamples(FrameResources &frame) {
  constexpr uint32_t stride = 8;
  const auto *source = frame.Targets.Get(Resource::GroundFieldRequests);
  if (source == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground field feedback image is missing"});
  }
  const auto &shape = source->Description();
  const uint64_t across = (uint64_t{shape.Width} + stride - 1) / stride;
  const uint64_t down = (uint64_t{shape.Height} + stride - 1) / stride;
  return frame.GroundFeedback.Configure(
      *Device_,
      *source,
      {.Stride = stride, .StorageBytesMost = 3 * across * down * sizeof(FieldPageFeedbackPixel)});
}

Native::Result<void>
SceneRenderer::EncodeGroundFieldFeedback([[maybe_unused]] const FrameContext &ctx,
                                         const PassRecording &into) {
  if (into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground field feedback has no command buffer"});
  }
  const auto encoded = ActiveFrame().GroundFeedback.Encode(
      *into.Commands, static_cast<uint32_t>(LastSubmittedCamera_.Serial % 64));
  if (!encoded) { return std::unexpected(encoded.error()); }
  return {};
}

size_t SceneRenderer::GroundFieldSamples() const noexcept {
  return ActiveFrame().GroundFeedback.Samples();
}

Native::Result<void> SceneRenderer::BeginGroundFieldReadback() {
  auto &feedback = ActiveFrame().GroundFeedback;
  if (feedback.Samples() == 0) { return {}; }
  const auto readback = feedback.BeginReadback();
  if (!readback && readback.error().Kind != Native::ErrorKind::Busy) {
    return std::unexpected(readback.error());
  }
  return {};
}

Native::Result<std::optional<size_t>>
SceneRenderer::TakeGroundFieldRequests(std::span<FieldPageAddress> into) {
  auto &feedback = ActiveFrame().GroundFeedback;
  if (into.size() < feedback.Samples()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "ground field feedback destination is too small"});
  }
  auto completed = feedback.Take();
  if (!completed) { return std::unexpected(completed.error()); }
  if (!*completed) { return std::optional<size_t>{}; }
  auto mapped = (**completed).Poll(*Device_);
  if (!mapped) { return std::unexpected(mapped.error()); }
  if (!*mapped) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "completed ground field readback lost its mapping"});
  }
  const auto decoded = DecodeFieldPageFeedback(**mapped, into);
  if (!decoded) { return std::unexpected(decoded.error()); }
  return std::optional{*decoded};
}
}
