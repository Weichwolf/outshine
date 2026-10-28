#include "SceneRenderer.h"
#include "DepthPyramid.h"
#include "EnvironmentSpecularLayout.h"
#include "ReadbackTransfer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kHalfSignBit = 0x8000u;
constexpr unsigned kHalfSignShift = 16u;
constexpr unsigned kHalfMantissaBits = 10u;
constexpr uint32_t kHalfExponentMask = 0x1Fu;
constexpr uint32_t kHalfMantissaMask = 0x3FFu;
constexpr uint32_t kHalfHiddenBit = 0x400u;
constexpr int kHalfExponentBias = 15;
constexpr int kSingleExponentBias = 127;
constexpr unsigned kSingleMantissaBits = 23u;
constexpr unsigned kMantissaWiden = kSingleMantissaBits - kHalfMantissaBits;
constexpr uint32_t kSingleInfinity = 0x7F800000u;

bool ReadSelectionQueues(Native::Device *device,
                         const GeometryResidency &resident,
                         KeptDraws &into) {
  for (const auto &[stream, state] :
       std::array{std::pair{GeometryResidency::Stream::ClusterChunkState, &into.RootWork},
                  std::pair{GeometryResidency::Stream::ClusterSelectedState, &into.ClusterWork}}) {
    Readback queue;
    const auto *buffer = resident.Buffer(stream).get();
    if (buffer == nullptr || queue.FromBuffer(device, buffer, sizeof(*state)) != ReadState::Ready) {
      return false;
    }
    std::memcpy(state, queue.Rows(), sizeof(*state));
    into.ReadbackBytes += sizeof(*state);
  }
  return true;
}

bool MaskRejectedRoots(Native::Device *device,
                       const GeometryResidency &resident,
                       std::span<const DrawBatch> batches,
                       std::span<uint32_t> into) {
  Readback roots;
  const auto *visible = resident.Buffer(GeometryResidency::Stream::ClusterVisible).get();
  const auto bytes = static_cast<uint32_t>(batches.size() * sizeof(uint32_t));
  if (visible == nullptr || roots.FromBuffer(device, visible, bytes) != ReadState::Ready) {
    return false;
  }
  const auto *status = reinterpret_cast<const uint32_t *>(roots.Rows());
  size_t logical = 0;
  for (size_t at = 0; at < batches.size(); ++at) {
    if (batches[at].JobCount > into.size() - logical) { return false; }
    if (status[at] == 0) { std::ranges::fill(into.subspan(logical, batches[at].JobCount), 0u); }
    logical += batches[at].JobCount;
  }
  return logical == into.size();
}

float HalfToFloat(uint16_t bits) {
  const auto held = static_cast<uint32_t>(bits);
  const uint32_t sign = (held & kHalfSignBit) << kHalfSignShift;
  const uint32_t exponent = (held >> kHalfMantissaBits) & kHalfExponentMask;
  uint32_t mantissa = held & kHalfMantissaMask;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      while ((mantissa & kHalfHiddenBit) == 0) {
        mantissa <<= 1u;
        ++shift;
      }
      mantissa &= kHalfMantissaMask;
      assembled = (static_cast<uint32_t>(kSingleExponentBias - kHalfExponentBias - shift + 1)
                   << kSingleMantissaBits) |
                  (mantissa << kMantissaWiden);
    }
  } else if (exponent == kHalfExponentMask) {
    assembled = kSingleInfinity | (mantissa << kMantissaWiden);
  } else {
    assembled = ((exponent + kSingleExponentBias - kHalfExponentBias) << kSingleMantissaBits) |
                (mantissa << kMantissaWiden);
  }
  assembled |= sign;
  float value = 0;
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

}

namespace Says {
constexpr auto kNoReadbackFrame = "no submitted image is available for colour readback";
constexpr auto kNoColourOutput = "the render plan has no final colour output";
constexpr auto kNoColourTexture = "the final colour output has no owned texture";
constexpr auto kColourReadbackFailed = "colour readback failed: ";

}

ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {
  ActiveState().WhyNot.clear();
  if (!ActiveState().Ready || !ActiveState().History.HasSubmittedFrame()) {
    ActiveState().WhyNot = Says::kNoReadbackFrame;
    return ReadState::Failed;
  }
  if (!ActiveState().Plan) {
    ActiveState().WhyNot = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  const auto source =
      ActiveState().Plan->Holds(Resource::FrameTex) ? Resource::FrameTex : Resource::Surface;
  if (!ActiveState().Plan->Holds(source)) {
    ActiveState().WhyNot = Says::kNoColourOutput;
    return ReadState::Failed;
  }
  const Native::Texture *const held = source == Resource::FrameTex
                                          ? ActiveFrame().Targets.Get(Resource::FrameTex)
                                          : ActiveFrame().Offscreen.get();
  if (held == nullptr) {
    ActiveState().WhyNot = Says::kNoColourTexture;
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       held,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    ActiveState().WhyNot = std::string(Says::kColourReadbackFailed) + read.WhyNot();
    return ReadState::Failed;
  }
  CollectCompletedFrameTimes();
  rgba.resize(static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) *
              4u);
  std::memcpy(rgba.data(), read.Rows(), rgba.size());
  const auto format = TextureFormatOf(ActiveState().Plan->Format(source));
  if (format == Native::Format::Bgra8Unorm || format == Native::Format::Bgra8Srgb) {
    for (size_t at = 0; at + 3 < rgba.size(); at += 4) { std::swap(rgba[at], rgba[at + 2]); }
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadDepth(std::vector<float> &depth) {
  if (!ActiveState().Ready || ActiveFrame().Targets.Get(Resource::SceneDepth) == nullptr) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       ActiveFrame().Targets.Get(Resource::SceneDepth),
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(ActiveFrame().Width) *
               static_cast<size_t>(ActiveFrame().Height));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSceneLinear(std::vector<float> &rgba) {
  const Native::Texture *source = LinearSource();
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  const bool wide = ActiveState().Plan->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       wide ? 16u : 8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 4u;
  rgba.resize(components);
  if (wide) {
    std::memcpy(rgba.data(), read.Rows(), components * sizeof(float));
    return ReadState::Ready;
  }

  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    rgba[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadShadowAtlas(std::vector<float> &depth) {
  if (!ActiveState().Ready || ActiveFrame().Shadow.Atlas() == nullptr ||
      !ActiveFrame().Shadow.HasSubmittedData()) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       ActiveFrame().Shadow.Atlas(),
                       {.WidthPx = kShadowAtlasPx, .HeightPx = kShadowAtlasPx},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize(static_cast<size_t>(kShadowAtlasPx) * static_cast<size_t>(kShadowAtlasPx));
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadGeometrySelection(KeptDraws &into, GeometryReadback detail) {
  into = {};
  const GeometryResidency &resident = ActiveContent().Geometry().Resident();
  const Native::Buffer *const args =
      resident.Buffer(GeometryResidency::Stream::DrawArguments).get();
  const uint32_t rows = ActiveContent().Geometry().ClusterBatchRows();
  if (!ActiveState().Ready || !ActiveFrame().Cull.HasAcceptedSelection() || args == nullptr ||
      rows == 0) {
    return ReadState::Failed;
  }
  if (!ReadSelectionQueues(Device_.get(), resident, into)) {
    into = {};
    return ReadState::Failed;
  }
  Readback read;
  const uint32_t bytes = rows * 5u * static_cast<uint32_t>(sizeof(uint32_t));
  if (read.FromBuffer(Device_.get(), args, bytes) != ReadState::Ready) {
    into = {};
    return ReadState::Failed;
  }
  const auto *const held = reinterpret_cast<const uint32_t *>(read.Rows());
  into.ReadbackBytes += bytes;
  if (detail == GeometryReadback::Indices) {
    into.Arguments.assign(held, held + static_cast<size_t>(rows) * 5u);
  }
  for (uint32_t at = 0; at < rows; ++at) {
    into.Indices += held[static_cast<size_t>(at) * 5];
    into.Batches += held[static_cast<size_t>(at) * 5] > 0u ? 1u : 0u;
  }
  if (detail == GeometryReadback::Counters) { return ReadState::Ready; }
  const uint32_t jobs = ActiveContent().Geometry().ClusterJobs();
  const Native::Buffer *const kept = resident.Buffer(GeometryResidency::Stream::ClusterKept).get();
  if (jobs > 0 && kept != nullptr) {
    Readback visibility;
    const uint32_t keptBytes = jobs * static_cast<uint32_t>(sizeof(uint32_t));
    if (visibility.FromBuffer(Device_.get(), kept, keptBytes) != ReadState::Ready) {
      into = {};
      return ReadState::Failed;
    }
    const auto *const values = reinterpret_cast<const uint32_t *>(visibility.Rows());
    into.Visibility.assign(values, values + jobs);
    into.ReadbackBytes += keptBytes;
    if (!MaskRejectedRoots(
            Device_.get(), resident, ActiveContent().Geometry().Drawn(), into.Visibility)) {
      into = {};
      return ReadState::Failed;
    }
    into.ReadbackBytes += rows * sizeof(uint32_t);
  }
  const Native::Buffer *const index = resident.Buffer(GeometryResidency::Stream::DrawIndex).get();
  if (index == nullptr || into.Indices == 0) { return ReadState::Ready; }
  Readback drawn;
  uint64_t end = 0;
  for (size_t at = 0; at < rows; ++at) {
    const auto first = into.Arguments[at * 5 + 2];
    const auto count = into.Arguments[at * 5];
    if (count != 0) { end = std::max(end, uint64_t{first} + count); }
  }
  const uint64_t indexBytes = end * sizeof(uint32_t);
  if (indexBytes > index->Description().Bytes ||
      indexBytes > std::numeric_limits<uint32_t>::max() ||
      drawn.FromBuffer(Device_.get(), index, static_cast<uint32_t>(indexBytes)) !=
          ReadState::Ready) {
    into = {};
    return ReadState::Failed;
  }
  const auto *indices = reinterpret_cast<const uint32_t *>(drawn.Rows());
  into.ReadbackBytes += indexBytes;
  into.DrawIndex.reserve(into.Indices);
  for (size_t at = 0; at < rows; ++at) {
    const auto first = into.Arguments[at * 5 + 2];
    const auto count = into.Arguments[at * 5];
    if (count != 0) {
      into.DrawIndex.insert(into.DrawIndex.end(), indices + first, indices + first + count);
    }
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadPyramid(PyramidDepths &into) {
  into = {};
  if (!ActiveState().Ready || ActiveFrame().DepthPyramid.Buffer() == nullptr) {
    return ReadState::Failed;
  }
  const PyramidShape &shape = ActiveFrame().DepthPyramid.Shape();
  const uint32_t texels = shape.Wide[0] * shape.High[0];
  if (texels == 0) { return ReadState::Failed; }
  const ReadState landed = ActiveFrame().PyramidRead.Poll();
  if (landed == ReadState::Failed) {
    return ActiveFrame().PyramidRead.Enqueue(Device_.get(),
                                             ActiveFrame().DepthPyramid.Buffer(),
                                             texels * static_cast<uint32_t>(sizeof(float))) ==
                   ReadState::Failed
               ? ReadState::Failed
               : ReadState::Pending;
  }
  if (landed == ReadState::Pending) { return ReadState::Pending; }
  const auto *const held = reinterpret_cast<const float *>(ActiveFrame().PyramidRead.Rows());
  double summed = 0.0;
  into.Nearest = held[0];
  into.Farthest = held[0];
  for (uint32_t at = 0; at < texels; ++at) {
    into.Nearest = std::max(held[at], into.Nearest);
    into.Farthest = std::min(held[at], into.Farthest);
    summed += static_cast<double>(held[at]);
  }
  into.Mean = static_cast<float>(summed / static_cast<double>(texels));
  ActiveFrame().PyramidRead.Release();
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadEnvironmentSpecular(std::vector<float> &rgba) {
  if (!ActiveState().Ready || !ActiveFrame().Atmosphere.Has(Resource::EnvironmentSpecular) ||
      !ActiveFrame().Atmosphere.Settled(Stage::EnvironmentSpecular)) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       ActiveFrame().Atmosphere.Texture(Resource::EnvironmentSpecular),
                       {.WidthPx = kEnvironmentWidth, .HeightPx = kEnvironmentHeight},
                       8u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  rgba.resize(static_cast<size_t>(kEnvironmentWidth) * kEnvironmentHeight * 4u);
  for (uint32_t row = 0; row < kEnvironmentHeight; ++row) {
    for (uint32_t component = 0; component < kEnvironmentWidth * 4u; ++component) {
      uint16_t bits = 0;
      std::memcpy(&bits,
                  read.Rows() + static_cast<size_t>(row) * read.RowBytes() +
                      component * sizeof(uint16_t),
                  sizeof(bits));
      rgba[(static_cast<size_t>(row) * kEnvironmentWidth * 4u) + component] = HalfToFloat(bits);
    }
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSkyIrradiance(std::span<float, kIrradianceFloats> out) {
  if (!ActiveState().Ready || !ActiveFrame().Atmosphere.Has(Resource::IrradianceBuffer) ||
      !ActiveFrame().Atmosphere.Settled(Stage::Irradiance)) {
    return ReadState::Failed;
  }
  Readback read;
  if (read.FromBuffer(Device_.get(),
                      ActiveFrame().Atmosphere.Irradiance(),
                      kIrradianceFloats * static_cast<uint32_t>(sizeof(float))) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  std::memcpy(out.data(), read.Rows(), kIrradianceFloats * sizeof(float));
  return ReadState::Ready;
}

namespace {
ReadState ReadHalfTarget(Native::Device *device,
                         const Native::Texture *source,
                         Extent extent,
                         uint32_t channels,
                         std::vector<float> &values) {
  if (source == nullptr) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(device, source, extent, channels * sizeof(uint16_t)) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = static_cast<size_t>(extent.WidthPx) * extent.HeightPx * channels;
  values.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    values[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}
}

ReadState SceneRenderer::ReadShadingNormal(std::vector<float> &xyz) {
  if (!ActiveState().Ready) { return ReadState::Failed; }
  return ReadHalfTarget(Device_.get(),
                        ActiveFrame().Targets.Get(Resource::SceneShadingNormal),
                        {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                        4,
                        xyz);
}

ReadState SceneRenderer::ReadSurfaceBase(std::vector<float> &rgba) {
  if (!ActiveState().Ready) { return ReadState::Failed; }
  return ReadHalfTarget(Device_.get(),
                        ActiveFrame().Targets.Get(Resource::SceneSurfaceBase),
                        {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                        4,
                        rgba);
}

ReadState SceneRenderer::ReadSurfaceMetalRough(std::vector<float> &rg) {
  if (!ActiveState().Ready) { return ReadState::Failed; }
  return ReadHalfTarget(Device_.get(),
                        ActiveFrame().Targets.Get(Resource::SceneSurfaceMetalRough),
                        {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                        2,
                        rg);
}

ReadState SceneRenderer::ReadSurface(SurfaceReadback &surface) {
  if (!ActiveState().Ready) { return ReadState::Failed; }
  constexpr std::array resources{Resource::SceneDepth,
                                 Resource::SceneShadingNormal,
                                 Resource::SceneSurfaceIdentity,
                                 Resource::SceneSurfaceBase,
                                 Resource::SceneSurfaceMetalRough};
  constexpr std::array formats{Native::Format::Depth32Float,
                               Native::Format::Rgba16Float,
                               Native::Format::Rgba32Float,
                               Native::Format::Rgba16Float,
                               Native::Format::Rg16Float};
  const Native::TextureRegion region{.Width = static_cast<uint32_t>(ActiveFrame().Width),
                                     .Height = static_cast<uint32_t>(ActiveFrame().Height)};
  std::array<TextureReadback, resources.size()> sources;
  std::array<ReadbackSlice, resources.size()> slices;
  for (size_t at = 0; at < resources.size(); ++at) {
    const auto *source = ActiveFrame().Targets.Get(resources[at]);
    if (source == nullptr || source->Description().Texels != formats[at]) {
      return ReadState::Failed;
    }
    sources[at] = {.Source = source, .Region = region};
  }
  surface.Reset();
  auto transfer = ReadbackTransfer::Start(*Device_, sources, slices);
  if (!transfer) { return ReadState::Failed; }
  const auto ready = transfer->Wait(*Device_, std::chrono::seconds(5));
  if (!ready || !*ready) { return ReadState::Failed; }
  surface.Transfer_ = std::move(*transfer);
  surface.Bytes_ = **ready;
  surface.Slices_ = slices;
  surface.Pixels_ = static_cast<size_t>(region.Width) * region.Height;
  return ReadState::Ready;
}

void SurfaceReadback::Reset() noexcept {
  Bytes_ = {};
  Pixels_ = 0;
  Transfer_.reset();
}

SurfacePixel SurfaceReadback::Read(size_t pixel) const noexcept {
  assert(pixel < Pixels_);
  const auto single = [&](size_t image, size_t component) {
    float value = 0;
    std::memcpy(&value,
                Bytes_.data() + Slices_[image].Range.Offset + component * sizeof(float),
                sizeof value);
    return value;
  };
  const auto half = [&](size_t image, size_t component) {
    uint16_t bits = 0;
    std::memcpy(&bits,
                Bytes_.data() + Slices_[image].Range.Offset + component * sizeof(uint16_t),
                sizeof bits);
    return HalfToFloat(bits);
  };
  return {.Normal = {{half(1, pixel * 4), half(1, pixel * 4 + 1), half(1, pixel * 4 + 2)}},
          .Depth = single(0, pixel),
          .Identity = single(2, pixel * 4),
          .Base = {{half(3, pixel * 4), half(3, pixel * 4 + 1), half(3, pixel * 4 + 2)}},
          .Metalness = half(4, pixel * 2),
          .Roughness = half(4, pixel * 2 + 1)};
}

ReadState SceneRenderer::ReadSceneVelocity(std::vector<float> &xy) {
  const Native::Texture *source = ActiveFrame().Targets.Get(Resource::SceneVelocity);
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       4u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState SceneRenderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  const Native::Texture *source = ActiveFrame().Targets.Get(Resource::SceneSurfaceIdentity);
  if (!ActiveState().Ready || (source == nullptr)) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.get(),
                       source,
                       {.WidthPx = ActiveFrame().Width, .HeightPx = ActiveFrame().Height},
                       16u) != ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components =
      static_cast<size_t>(ActiveFrame().Width) * static_cast<size_t>(ActiveFrame().Height) * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

}
