#include "FrameCoordinator.h"
#include "EngineState.h"
#include <chrono>
#include <expected>
#include <string>
#include <utility>

namespace outshine {

namespace Says {
constexpr auto kFrameAlreadyOpen = "a frame is already open on this engine";
constexpr auto kInvalidFrameExtent =
    "frame extent must be zero in both dimensions or positive in both";
}

namespace {
Result ValidateFrameExtent(Extent frame, const EngineRenderState &picture) {
  const bool defaultTarget = frame.WidthPx == 0 && frame.HeightPx == 0;
  if (!defaultTarget && (frame.WidthPx <= 0 || frame.HeightPx <= 0)) {
    return std::unexpected(std::string(Says::kInvalidFrameExtent));
  }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != picture.Frame.WidthPx || frame.HeightPx != picture.Frame.HeightPx)) {
    return std::unexpected("this engine stands on a " + std::to_string(picture.Frame.WidthPx) +
                           "x" + std::to_string(picture.Frame.HeightPx) +
                           " canvas and was asked to draw " + std::to_string(frame.WidthPx) + "x" +
                           std::to_string(frame.HeightPx) +
                           " -- a canvas is declared before a scenario stands on it");
  }
  return {};
}
}

Result FrameCoordinator::Render(Extent frame) {
  [[maybe_unused]] const auto logs = State_.Logs();
  if (const auto valid = ValidateFrameExtent(frame, State_.Picture); !valid) {
    return std::unexpected(valid.error());
  }
  if (!State_.EnsureRuntimeScene()) { return std::unexpected(State_.Error); }
  const auto began = std::chrono::steady_clock::now();
  if (!State_.Picture.Standing->Draw(State_.Error)) { return std::unexpected(State_.Error); }
  if (Open()) { Phase_ = Phase::DrawSucceeded; }
  State_.PublishRenderedFrame(began);
  return {};
}

Result FrameCoordinator::Begin() {
  [[maybe_unused]] const auto logs = State_.Logs();
  if (Open()) { return std::unexpected(std::string(Says::kFrameAlreadyOpen)); }
  if (!State_.EnsureRuntimeScene()) { return std::unexpected(State_.Error); }
  if (!State_.Picture.Standing) {
    return std::unexpected(std::string("a frame is begun over a scenario, and none stands"));
  }
  Phase_ = Phase::Open;
  return {};
}

Result FrameCoordinator::End() {
  [[maybe_unused]] const auto logs = State_.Logs();
  if (!Open()) { return std::unexpected(std::string("a frame was ended that was never begun")); }
  const auto completed = std::exchange(Phase_, Phase::Closed);
  if (completed == Phase::DrawSucceeded || !State_.Picture.Standing ||
      !State_.Picture.Device.Presents()) {
    return {};
  }
  return Render({});
}

Result FrameCoordinator::Flush() {
  [[maybe_unused]] const auto logs = State_.Logs();
  if (!State_.Picture.Standing) { return {}; }
  if (!State_.Picture.Device.Settle(State_.Error)) { return std::unexpected(State_.Error); }
  State_.PublishGpuFrameMeasurements(true);
  return {};
}

Result Engine::render(Extent frame) {
  return S_->Frames.Render(frame);
}

Result Engine::beginFrame() {
  return S_->Frames.Begin();
}

Result Engine::endFrame() {
  return S_->Frames.End();
}

Result Engine::flushAndWait() {
  return S_->Frames.Flush();
}

Extent Engine::canvas() const {
  return S_->Picture.Targeted ? S_->Picture.Frame : Extent{};
}

bool Engine::camera(Camera &out) const {
  if (!S_->Picture.Standing) { return false; }
  Render::CameraOf(S_->Picture.Standing->Aimed(), out);
  return true;
}

bool Engine::presenting() const {
  return S_->Picture.Device.Presents();
}

}
