#include "Device.h"
#include "Check.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

int main() {
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  constexpr uint32_t width = 64;
  constexpr uint32_t height = 4;
  constexpr size_t pixels = static_cast<size_t>(width) * height;
  constexpr uint64_t bytes = pixels * sizeof(float);
  const auto device = CreateDevice();
  CHECK(device, "native depth copy uses a real device");
  if (!device) { return Report(); }
  auto &gpu = **device;
  const auto usage = TextureUse::DepthAttachment | TextureUse::CopySource |
                     TextureUse::CopyDestination | TextureUse::Sampled;
  auto source = gpu.CreateTexture(
      {.Width = width, .Height = height, .Texels = Format::Depth32Float, .Use = usage});
  auto target = gpu.CreateTexture(
      {.Width = width, .Height = height, .Texels = Format::Depth32Float, .Use = usage});
  auto output = gpu.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback});
  CHECK(source && target && output, "depth attachments and readback allocate");
  if (!source || !target || !output) { return Report(); }
  auto commands = gpu.Begin();
  CHECK(commands, "native commands begin");
  if (!commands) { return Report(); }
  const auto pass = (*commands)->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{
           .Image = {.Image = source->get()}, .Begin = Load::Clear, .Clear = 0.375f}});
  CHECK(pass, "depth-only clear begins");
  if (!pass) { return Report(); }
  CHECK((*commands)->EndRenderPass(), "depth-only clear ends");
  constexpr TextureRegion region{.Width = width, .Height = height};
  CHECK((*commands)->Copy(**source, region, **target, region),
        "depth copies into a distinct attachment");
  CHECK(
      (*commands)->Copy(**target, **output, {.RowBytes = width * sizeof(float), .Region = region}),
      "copied depth can be read back");
  auto completed = gpu.Submit(std::move(*commands));
  CHECK(completed, "depth copy submits");
  if (!completed) { return Report(); }
  auto ready = gpu.Wait(*completed, std::chrono::seconds(5));
  CHECK(ready && *ready == CompletionStatus::Ready, "depth copy completes");
  if (!ready || *ready != CompletionStatus::Ready) { return Report(); }
  auto read = gpu.MapRead(**output, {.Bytes = bytes});
  CHECK(read && read->size() == bytes, "completed depth can be mapped");
  if (read && read->size() == bytes) {
    bool exact = true;
    for (size_t at = 0; at < pixels; ++at) {
      float value = 0;
      std::memcpy(&value, read->data() + at * sizeof(float), sizeof(value));
      exact &= value == 0.375f;
    }
    CHECK(exact, "all copied depth values retain their original bits");
    CHECK(gpu.Unmap(**output), "depth readback unmaps");
  }
  return Report();
}
