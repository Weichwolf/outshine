#include "GpuSupport.h"
#include "Calibration.h"
#include "Parameters.inc"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Diagnostics::Calibration {
namespace {
Native::Result<bool> Verify(Native::Device &device,
                            const Native::Texture &image,
                            Native::Buffer &readback,
                            std::span<const std::byte> expected) {
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  Native::TextureTransfer transfer{};
  transfer.RowBytes = uint64_t{CALIBRATION_WIDTH} * 4;
  transfer.Region.Width = CALIBRATION_WIDTH;
  transfer.Region.Height = CALIBRATION_HEIGHT;
  const auto copied = (*commands)->Copy(image, readback, transfer);
  if (!copied) { return std::unexpected(copied.error()); }
  const auto finished = Finish(device, std::move(*commands));
  if (!finished) { return std::unexpected(finished.error()); }
  const auto mapped = device.MapRead(readback, {.Bytes = expected.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  const bool same = std::memcmp(mapped->data(), expected.data(), expected.size()) == 0;
  const auto unmapped = device.Unmap(readback);
  if (!unmapped) { return std::unexpected(unmapped.error()); }
  return same;
}
}

Native::Result<Workload> Raster(Native::Device &device, Shaders shaders) {
  constexpr uint64_t bytes = uint64_t{CALIBRATION_WIDTH} * CALIBRATION_HEIGHT * 4u;
  std::vector<uint8_t> expected(bytes);
  for (uint32_t y = 0; y < CALIBRATION_HEIGHT; ++y) {
    for (uint32_t x = 0; x < CALIBRATION_WIDTH; ++x) {
      const auto value = RasterValue(x, y);
      const size_t at = (size_t{y} * CALIBRATION_WIDTH + x) * 4;
      expected[at] = static_cast<uint8_t>(value);
      expected[at + 1] = static_cast<uint8_t>(value >> 8u);
      expected[at + 2] = static_cast<uint8_t>(value >> 16u);
      expected[at + 3] = std::numeric_limits<uint8_t>::max();
    }
  }
  auto image = device.CreateTexture(
      {.Width = CALIBRATION_WIDTH,
       .Height = CALIBRATION_HEIGHT,
       .Texels = Native::Format::Rgba8Unorm,
       .Use = Native::TextureUse::ColourAttachment | Native::TextureUse::CopySource});
  if (!image) { return std::unexpected(image.error()); }
  auto readback = device.CreateBuffer({.Bytes = bytes,
                                       .Use = Native::BufferUse::CopyDestination,
                                       .Placement = Native::Memory::Readback});
  if (!readback) { return std::unexpected(readback.error()); }
  const std::array targets{Native::ColourTarget{}};
  Native::GraphicsDescription description{};
  description.Vertex.Spirv = shaders.Vertex;
  description.Fragment = Native::ShaderCode{.Spirv = shaders.Fragment, .Entry = "main"};
  description.Colours = targets;
  description.Culling = Native::Cull::None;
  description.Label = "calibration-raster";
  auto pipeline = device.CreateGraphicsPipeline(description);
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  Workload work{};
  work.Name = "gpu_raster_1280x720";
  work.TimeBasis = "submission_gpu_ns";
  work.Elements = uint64_t{CALIBRATION_WIDTH} * CALIBRATION_HEIGHT;
  work.Iterations = CALIBRATION_RASTER_STEPS;
  work.LogicalOperations = work.Elements * work.Iterations * 4u;
  work.WorkingSetBytes = bytes;
  work.Note =
      "One fullscreen triangle, 64 uint mix steps per requested pixel, RGBA8 output. "
      "Complete coordinate-derived pixels checked outside timing; not a world/frame-budget score.";
  for (size_t round = 0; round <= CALIBRATION_SAMPLES; ++round) {
    auto sample = Measure(device, [&](Native::Commands &commands) -> Native::Result<void> {
      Native::ColourAttachment attachment{};
      attachment.Image.Image = image->get();
      attachment.Begin = Native::Load::Discard;
      attachment.End = Native::Store::Store;
      const std::array colours{attachment};
      Native::RenderPassDescription pass{};
      pass.Colours = colours;
      const auto begun = commands.BeginRenderPass(pass);
      if (!begun) { return begun; }
      Native::DrawDescription draw{};
      draw.State.Pipeline = pipeline->get();
      draw.State.Area.Width = CALIBRATION_WIDTH;
      draw.State.Area.Height = CALIBRATION_HEIGHT;
      draw.State.Clip.Width = CALIBRATION_WIDTH;
      draw.State.Clip.Height = CALIBRATION_HEIGHT;
      draw.Range.Count = 3;
      const auto drawn = commands.Draw(draw);
      if (!drawn) { return drawn; }
      return commands.EndRenderPass();
    });
    if (!sample) { return std::unexpected(sample.error()); }
    const auto verified = Verify(device, **image, **readback, std::as_bytes(std::span(expected)));
    if (!verified) { return std::unexpected(verified.error()); }
    if (!*verified) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "calibration raster pixels differ"});
    }
    if (round != 0) { work.Samples.push_back(*sample); }
  }
  work.Checksum = Checksum(std::as_bytes(std::span(expected)));
  return work;
}
}
