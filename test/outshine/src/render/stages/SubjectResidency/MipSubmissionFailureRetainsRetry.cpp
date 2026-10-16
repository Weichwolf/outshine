#include "Check.h"
#include "SubjectResidency.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Faults final : NativeDeviceProxy {
  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  bool Reject = false;
  unsigned Rejected = 0;

  [[nodiscard]] Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (Reject) {
      Reject = false;
      ++Rejected;
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "injected mip submission failure"});
    }
    return Backing().Submit(std::move(commands));
  }
};
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Faults device(*actual);
  {
    SubjectResidency residency;
    Require(residency.StandsOn(&device));
    const std::array<uint8_t, 16> pixels = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    const SubjectTexture texture{.Rgba = pixels.data(), .Width = 2, .Height = 2, .Uv = {}};
    device.Reject = true;
    const auto failed =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(!failed && device.Rejected == 1 && !device.Reject,
          "a complete mip submission failure returns no partially usable image");
    const auto retry =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(retry && retry->Image && retry->Sample,
          "an immediate mip upload retry returns a complete sampled image");
    device.Reject = true;
    const auto defaultFailed =
        residency.Upload({}, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(!defaultFailed && device.Rejected == 2 && !device.Reject,
          "a failed default upload is not published for reuse");
    const auto defaultRetry =
        residency.Upload({}, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    const auto defaultShared =
        residency.Upload({}, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(defaultRetry && defaultShared && defaultRetry->Image.get() == defaultShared->Image.get(),
          "a repaired default is shared only after a complete retry");
  }
  return Report();
}
