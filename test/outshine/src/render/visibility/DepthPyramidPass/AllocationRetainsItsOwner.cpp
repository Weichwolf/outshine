#include "DepthPyramidPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr Extent kExtent{.WidthPx = 16, .HeightPx = 16};

class PyramidDevice final : public NativeDeviceProxy {
public:
  explicit PyramidDevice(Device &device) : NativeDeviceProxy(device), Caps(device.Limits()) {}
  enum class Reply { Fail, Empty, Create };
  Reply Buffers = Reply::Fail;
  Reply Pipelines = Reply::Fail;
  size_t BufferRequests = 0, PipelineRequests = 0;
  Capabilities Caps;

  [[nodiscard]] const Capabilities &Limits() const noexcept override { return Caps; }

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    ++BufferRequests;
    if (Buffers == Reply::Fail) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "buffer refused"});
    }
    if (Buffers == Reply::Empty) { return std::unique_ptr<Buffer>{}; }
    return NativeDeviceProxy::CreateBuffer(description, label);
  }

  Result<std::unique_ptr<ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> spirv, ComputeOptions options = {}) override {
    ++PipelineRequests;
    if (Pipelines == Reply::Fail) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "pipeline refused"});
    }
    if (Pipelines == Reply::Empty) { return std::unique_ptr<ComputePipeline>{}; }
    return NativeDeviceProxy::CreateComputePipeline(spirv, options);
  }
};

void DeviceBounds(Device &native) {
  PyramidDevice device(native);
  device.Buffers = PyramidDevice::Reply::Create;
  DepthPyramidPass pass;
  CHECK(!pass.Allocate(device, {.WidthPx = 0, .HeightPx = 16}) &&
            !pass.Allocate(device, {.WidthPx = 16, .HeightPx = -1}),
        "nonpositive hierarchy dimensions are rejected before native allocation");
  device.Caps.MaxTextureExtent = 8;
  CHECK(!pass.Allocate(device, kExtent), "source bounds respect the device texture limit");
  device.Caps.MaxTextureExtent = native.Limits().MaxTextureExtent;
  device.Caps.MaxStorageBufferBytes = 1;
  CHECK(!pass.Allocate(device, kExtent), "hierarchy bytes respect the actual storage limit");
  device.Caps.MaxStorageBufferBytes = std::numeric_limits<uint64_t>::max();
  device.Caps.MaxTextureExtent = std::numeric_limits<int>::max();
  constexpr int largest = std::numeric_limits<int>::max();
  CHECK(!pass.Allocate(device, {.WidthPx = largest, .HeightPx = largest}) &&
            device.BufferRequests == 0 && pass.Buffer() == nullptr,
        "four-level texel offsets cannot overflow even when a device advertises huge extents");
}
}

int main() {
  const auto native = Require(CreateDevice({.Validation = true}));
  DeviceBounds(*native);
  PyramidDevice device(*native);
  DepthPyramidPass pass;
  CHECK(pass.Buffer() == nullptr && pass.Shape().Texels == 0 && !pass.Ready(),
        "an undemanded hierarchy owns no buffer, shape or bindings");
  const auto refused = pass.Allocate(device, kExtent);
  CHECK(!refused && refused.error().Message == "buffer refused" && !pass.Buffer(),
        "native allocation failure leaves the pass unallocated");
  device.Buffers = PyramidDevice::Reply::Empty;
  const auto empty = pass.Allocate(device, kExtent);
  CHECK(!empty && empty.error().Kind == ErrorKind::Driver && pass.Shape().Texels == 0,
        "a successful result without a buffer cannot publish hierarchy metadata");
  device.Buffers = PyramidDevice::Reply::Create;
  Require(pass.Allocate(device, kExtent));
  const auto *const buffer = pass.Buffer();
  const auto shape = PyramidOver({.WidthPx = 16, .HeightPx = 16});
  CHECK(buffer && buffer->Description().Bytes == uint64_t{shape.Texels} * sizeof(float) &&
            pass.Shape().At == shape.At && !pass.Ready(),
        "allocated storage and offsets belong to one unconfigured hierarchy");
  const auto source = Require(native->CreateTexture(
      {.Width = 16, .Height = 16, .Texels = Format::R32Float, .Use = TextureUse::Sampled}));
  const auto sampler = Require(native->CreateSampler({}));
  const auto pipeline = pass.Configure(source.get(), sampler.get());
  CHECK(!pipeline && pipeline.error().Message == "pipeline refused" && pass.Buffer() == buffer &&
            !pass.Ready(),
        "pipeline failure retains the allocated hierarchy for retry");
  device.Pipelines = PyramidDevice::Reply::Empty;
  CHECK(!pass.Configure(source.get(), sampler.get()) && !pass.Ready(),
        "an empty pipeline result cannot make a hierarchy ready");
  device.Pipelines = PyramidDevice::Reply::Create;
  Require(pass.Configure(source.get(), sampler.get()));
  Require(pass.Allocate(device, kExtent));
  CHECK(pass.Ready() && pass.Buffer() == buffer && device.BufferRequests == 3 &&
            device.PipelineRequests == 3,
        "repeated allocation reuses the complete configured hierarchy");
  PyramidDevice another(*native);
  CHECK(!pass.Allocate(another, kExtent) &&
            !pass.Allocate(device, {.WidthPx = 8, .HeightPx = 16}) &&
            !pass.Configure(nullptr, sampler.get()) && pass.Ready() && pass.Buffer() == buffer &&
            another.BufferRequests == 0 && device.BufferRequests == 3,
        "foreign owners, mismatched extents and bad bindings preserve the live hierarchy");
  return Report();
}
