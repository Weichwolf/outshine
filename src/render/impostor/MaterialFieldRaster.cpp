#include "MaterialFieldRaster.h"
#include "MaterialFieldRasterInput.h"
#include "MaterialFieldPass.h"
#include "MaterialPacking.h"
#include "LightingPacking.h"
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
struct alignas(16) RasterFrame {
  Vec4f RightExtent, UpExtent, ForwardNear, FarRaster;
};

static_assert(sizeof(RasterFrame) == 64 && alignof(RasterFrame) == 16);
static_assert(offsetof(RasterFrame, RightExtent) == 0);
static_assert(offsetof(RasterFrame, UpExtent) == 16);
static_assert(offsetof(RasterFrame, ForwardNear) == 32);
static_assert(offsetof(RasterFrame, FarRaster) == 3 * sizeof(Vec4f));

Vec4f Direction(const Vec3 &direction, double tail) {
  return {{static_cast<float>(direction[0]),
           static_cast<float>(direction[1]),
           static_cast<float>(direction[2]),
           static_cast<float>(tail)}};
}
}

Native::Result<std::unique_ptr<MaterialFieldRaster>> MaterialFieldRaster::Prepare(
    Native::Device &device, Native::Commands &commands, const MaterialFieldRasterInput &input) {
  const uint64_t bytes = input.Words.size() * sizeof(MaterialFieldRasterInput::Word);
  auto result = std::make_unique<MaterialFieldRaster>();
  auto pixels = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::Storage | Native::BufferUse::CopyDestination});
  if (!pixels) { return std::unexpected(pixels.error()); }
  result->Pixels = std::move(*pixels);
  auto staging = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  result->Staging = std::move(*staging);
  auto mapped = device.MapWrite(*result->Staging, {.Bytes = bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), input.Words.data(), static_cast<size_t>(bytes));
  if (auto unmapped = device.Unmap(*result->Staging); !unmapped) {
    return std::unexpected(unmapped.error());
  }
  auto copied = commands.Copy(*result->Staging, *result->Pixels, {.Bytes = bytes});
  if (!copied) { return std::unexpected(copied.error()); }
  auto empty = device.CreateTexture(
      {.Width = 1,
       .Height = 1,
       .Texels = Native::Format::Rgba8Unorm,
       .Use = Native::TextureUse::Sampled | Native::TextureUse::ColourAttachment});
  if (!empty) { return std::unexpected(empty.error()); }
  result->Empty = std::move(*empty);
  const std::array attachments{Native::ColourAttachment{.Image = {.Image = result->Empty.get()},
                                                        .Begin = Native::Load::Clear}};
  auto begun = commands.BeginRenderPass({.Colours = attachments, .Depth = {}});
  if (!begun) { return std::unexpected(begun.error()); }
  auto ended = commands.EndRenderPass();
  if (!ended) { return std::unexpected(ended.error()); }
  return result;
}

Native::Result<void> MaterialFieldRaster::Encode(Native::Commands &commands,
                                                 const Native::GraphicsPipeline &pipeline,
                                                 const Native::Sampler &sampling,
                                                 const MaterialFieldView &view,
                                                 const FrameContext &frame,
                                                 Extent extent) const {
  const auto &camera = view.Camera;
  const RasterFrame raster{.RightExtent = Direction(camera.Right, camera.XMagM),
                           .UpExtent = Direction(camera.Up, camera.YMagM),
                           .ForwardNear = Direction(camera.Forward, camera.ZNearM),
                           .FarRaster = {{static_cast<float>(camera.ZFarM),
                                          static_cast<float>(extent.WidthPx),
                                          static_cast<float>(extent.HeightPx),
                                          0}}};
  const auto lighting = PackLightingUniform(view.Lights, view.Environment, frame);
  const auto material = PackMaterial(MaterialBinding{}, 0);
  const auto uniform = [](const auto &value) {
    return Native::Uniform{.Bytes = std::as_bytes(std::span(&value, 1))};
  };
  const Native::SampledTexture empty{.Image = {.Image = Empty.get()}, .Sampling = &sampling};
  const std::array bindings{
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value =
                          Native::StorageBuffer{.Data = Pixels.get(),
                                                .Range = {.Bytes = Pixels->Description().Bytes}}},
      Native::Binding{.Set = 1, .Slot = 0, .Value = uniform(raster)},
      Native::Binding{.Set = 2, .Slot = 0, .Value = empty},
      Native::Binding{.Set = 2, .Slot = 1, .Value = empty},
      Native::Binding{.Set = 3, .Slot = 0, .Value = uniform(material)},
      Native::Binding{.Set = 3, .Slot = 1, .Value = uniform(lighting)}};
  return commands.Draw({.State = {.Pipeline = &pipeline,
                                  .Bindings = bindings,
                                  .Vertices = {},
                                  .Area = {.Width = static_cast<float>(extent.WidthPx),
                                           .Height = static_cast<float>(extent.HeightPx)},
                                  .Clip = {.Width = static_cast<uint32_t>(extent.WidthPx),
                                           .Height = static_cast<uint32_t>(extent.HeightPx)}},
                        .Range = {.Count = 3}});
}
}
