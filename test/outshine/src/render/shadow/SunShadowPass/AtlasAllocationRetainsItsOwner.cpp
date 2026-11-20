#include "SunShadowPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <memory>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class AtlasDevice final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  enum class Reply { Fail, FailObjects, Empty, Allocate };
  Reply Next = Reply::Fail;
  std::vector<TextureDescription> Requests;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    Requests.push_back(description);
    if (Next == Reply::Fail ||
        (Next == Reply::FailObjects && (static_cast<uint32_t>(description.Use) &
                                        static_cast<uint32_t>(TextureUse::Sampled)) == 0)) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "atlas allocation refused"});
    }
    if (Next == Reply::Empty) { return std::unique_ptr<Texture>{}; }
    return NativeDeviceProxy::CreateTexture(description, label);
  }
};
}

int main() {
  const auto native = Require(CreateDevice({.Validation = true}));
  AtlasDevice device(*native);
  SunShadowPass pass;
  CHECK(pass.Atlas() == nullptr && !pass.HasSubmittedData(),
        "an unrequested shadow pass owns no texture or submitted cache");
  const auto failed = pass.Allocate(device);
  CHECK(!failed && failed.error().Message == "atlas allocation refused" &&
            pass.Atlas() == nullptr && device.Requests.size() == 1,
        "failed native allocation leaves no published target");
  device.Next = AtlasDevice::Reply::Empty;
  const auto empty = pass.Allocate(device);
  CHECK(!empty && empty.error().Kind == ErrorKind::Driver && pass.Atlas() == nullptr,
        "a successful native result without a texture is rejected");
  device.Next = AtlasDevice::Reply::FailObjects;
  CHECK(!pass.Allocate(device) && pass.Atlas() == nullptr && device.Requests.size() == 4,
        "failure of the object atlas does not publish a partial shadow pass");
  device.Next = AtlasDevice::Reply::Allocate;
  Require(pass.Allocate(device));
  const auto *const atlas = pass.Atlas();
  CHECK(atlas != nullptr && !pass.HasSubmittedData(),
        "a successful allocation belongs to the pass without inventing shadow history");
  const auto depthUses = TextureUse::DepthAttachment | TextureUse::CopySource;
  const auto combinedUses = depthUses | TextureUse::Sampled | TextureUse::CopyDestination;
  const auto matches = [](const TextureDescription &requested, UseMask<TextureUse> uses) {
    return requested.Width == kShadowAtlasPx && requested.Height == kShadowAtlasPx &&
           requested.Texels == Format::Depth32Float &&
           static_cast<uint32_t>(requested.Use) == static_cast<uint32_t>(uses);
  };
  CHECK(device.Requests.size() == 6 && matches(device.Requests[4], combinedUses) &&
            matches(device.Requests[5], depthUses),
        "the combined atlas supports sampling and copies, and the object atlas supplies depth");
  Require(pass.Allocate(device));
  CHECK(pass.Atlas() == atlas && device.Requests.size() == 6,
        "repeated demand reuses the atlases without another allocation");
  AtlasDevice another(*native);
  another.Next = AtlasDevice::Reply::Allocate;
  const auto foreign = pass.Allocate(another);
  CHECK(!foreign && foreign.error().Kind == ErrorKind::Invalid && another.Requests.empty() &&
            pass.Atlas() == atlas && !pass.HasSubmittedData(),
        "another device cannot replace the live target or its cache state");
  Require(pass.Allocate(device));
  CHECK(pass.Atlas() == atlas && device.Requests.size() == 6,
        "failed replacement leaves the original owner reusable");
  return Report();
}
