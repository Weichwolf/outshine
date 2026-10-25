#include "EngineState.h"
#include "FrameReadback.h"
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace outshine {

namespace Says {
constexpr auto kColourReadbackIsBytes = "Colour requires the RGBA8 readback overload";
constexpr auto kUnknownReadbackBuffer = "unknown readback buffer";
}

Result Engine::inspect() {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (!S_->EnsureRuntimeScene()) { return std::unexpected(S_->Error); }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be inspected -- a scenario is declared before a frame carries "
                "anything a readback could tell";
    return std::unexpected(S_->Error);
  }
  S_->Published.BeginFrame();
  S_->Inspected();
  return {};
}

Result Engine::readPixels(std::vector<uint8_t> &rgba) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (!S_->Picture.Standing) {
    return std::unexpected(std::string(
        "nothing stands to be read -- a scenario is declared before a frame carries pixels"));
  }
  if (Render::ReadFrame(S_->Picture.Device, rgba, S_->Error)) {
    S_->PublishGpuFrameMeasurements(true);
    return {};
  }
  return std::unexpected(S_->Error);
}

Result Engine::readPixels(Buffer which, std::vector<float> &out) {
  [[maybe_unused]] const auto logs = S_->Logs();
  switch (which) {
    case Buffer::Linear:
    case Buffer::Depth:
    case Buffer::ShadingNormal:
    case Buffer::SurfaceIdentity:
    case Buffer::Velocity: break;
    case Buffer::Colour: return std::unexpected(std::string(Says::kColourReadbackIsBytes));
    default: return std::unexpected(std::string(Says::kUnknownReadbackBuffer));
  }
  if (!S_->Picture.Standing) {
    return std::unexpected(std::string(
        "nothing stands to be read -- a scenario is declared before a frame carries pixels"));
  }
  if (Render::ReadFrame(S_->Picture.Device, which, out, S_->Error)) { return {}; }
  return std::unexpected(S_->Error);
}

}
