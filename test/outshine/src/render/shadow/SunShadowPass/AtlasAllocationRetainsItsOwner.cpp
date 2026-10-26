#include "SunShadowPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <memory>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <string_view>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class AtlasDevice final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  enum class Reply { Fail, Empty, Allocate };
  Reply Next = Reply::Fail;
  size_t Requests = 0;
  TextureDescription Requested;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    ++Requests;
    Requested = description;
    if (Next == Reply::Fail) {
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
            pass.Atlas() == nullptr && device.Requests == 1,
        "failed native allocation leaves no published target");
  device.Next = AtlasDevice::Reply::Empty;
  const auto empty = pass.Allocate(device);
  CHECK(!empty && empty.error().Kind == ErrorKind::Driver && pass.Atlas() == nullptr,
        "a successful native result without a texture is rejected");
  device.Next = AtlasDevice::Reply::Allocate;
  Require(pass.Allocate(device));
  const auto *const atlas = pass.Atlas();
  CHECK(atlas != nullptr && !pass.HasSubmittedData(),
        "a successful allocation belongs to the pass without inventing shadow history");
  const auto &requested = device.Requested;
  const auto uses = TextureUse::DepthAttachment | TextureUse::Sampled | TextureUse::CopySource;
  CHECK(requested.Width == kShadowAtlasPx && requested.Height == kShadowAtlasPx &&
            requested.Texels == Format::Depth32Float &&
            static_cast<uint32_t>(requested.Use) == static_cast<uint32_t>(uses),
        "the owned atlas retains depth, sampling and readback contracts");
  Require(pass.Allocate(device));
  CHECK(pass.Atlas() == atlas && device.Requests == 3,
        "repeated demand reuses the same texture without another allocation");
  AtlasDevice another(*native);
  another.Next = AtlasDevice::Reply::Allocate;
  const auto foreign = pass.Allocate(another);
  CHECK(!foreign && foreign.error().Kind == ErrorKind::Invalid && another.Requests == 0 &&
            pass.Atlas() == atlas && !pass.HasSubmittedData(),
        "another device cannot replace the live target or its cache state");
  Require(pass.Allocate(device));
  CHECK(pass.Atlas() == atlas && device.Requests == 3,
        "failed replacement leaves the original owner reusable");
  return Report();
}
