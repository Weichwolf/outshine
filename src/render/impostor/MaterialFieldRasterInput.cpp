#include "MaterialFieldRasterInput.h"
#include "ImpostorAtlas.h"
#include "ImpostorViewFrame.h"
#include "ImpostorCaptureMaterial.h"
#include "math/Octahedral.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace outshine::Render {
static_assert(static_cast<int>(ScatteringModel::MetallicRoughness) == 0 &&
              static_cast<int>(ScatteringModel::DiffuseThinSheet) == 1);

std::expected<MaterialFieldRasterInput, std::string>
MaterialFieldRasterInput::Prepare(const ImpostorAtlas &atlas, Request request) {
  if (request.View >= atlas.Views().size()) {
    return std::unexpected("stored material view unavailable");
  }
  const auto &source = atlas.Views()[request.View];
  const auto frame = ImpostorViewFrame::From(source.TowardEye);
  const size_t count = source.Texels.size();
  constexpr size_t pixelBytes = 2 * sizeof(Word);
  if (!frame || count == 0 ||
      !std::ranges::any_of(source.Texels, [](const auto &sample) { return sample.Surface != 0; }) ||
      count > request.BytesMost / pixelBytes ||
      atlas.Surfaces().size() > (request.BytesMost - count * pixelBytes) / sizeof(Word)) {
    return std::unexpected("stored material raster exceeds source admission");
  }
  MaterialFieldRasterInput result;
  result.Words.resize(2 * count + atlas.Surfaces().size());
  for (size_t at = 0; at < atlas.Surfaces().size(); ++at) {
    const auto &material = atlas.Surfaces()[at];
    if (!CapturedImpostorMaterial(material)) {
      return std::unexpected("stored raster requires captured PBR materials");
    }
    result.Words[2 * count + at].Values = {
        std::bit_cast<uint32_t>(material.DiffuseTransmission[0]),
        std::bit_cast<uint32_t>(material.DiffuseTransmission[1]),
        std::bit_cast<uint32_t>(material.DiffuseTransmission[2]),
        std::bit_cast<uint32_t>(static_cast<float>(material.Scattering))};
  }
  for (size_t at = 0; at < count; ++at) {
    const auto &sample = source.Texels[at];
    if (sample.Surface == 0) { continue; }
    const auto &material = atlas.Surfaces()[sample.Surface - 1];
    const auto parameters =
        source.Materials.empty()
            ? ImpostorAtlas::SurfaceSample{.BaseColour = {{material.BaseColour[0],
                                                           material.BaseColour[1],
                                                           material.BaseColour[2]}},
                                           .Roughness = material.Roughness,
                                           .Metalness = material.Metalness}
            : source.Materials[at];
    result.Words[2 * at].Values = {PackedPair(OctFolded(sample.Normal.Axis)),
                                   std::bit_cast<uint32_t>(sample.Depth),
                                   std::bit_cast<uint32_t>(parameters.Metalness),
                                   sample.Surface};
    result.Words[2 * at + 1].Values = {std::bit_cast<uint32_t>(parameters.BaseColour[0]),
                                       std::bit_cast<uint32_t>(parameters.BaseColour[1]),
                                       std::bit_cast<uint32_t>(parameters.BaseColour[2]),
                                       std::bit_cast<uint32_t>(parameters.Roughness)};
  }
  auto &camera = result.Capture;
  camera.EyeM = atlas.CentreM() + source.TowardEye * (3 * atlas.HalfExtentM());
  camera.Right = frame->Right;
  camera.Up = frame->Up;
  camera.Forward = source.TowardEye * -1.0;
  camera.Kind = CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = atlas.HalfExtentM();
  camera.ZNearM = atlas.HalfExtentM();
  camera.ZFarM = 5 * atlas.HalfExtentM();
  return result;
}
}
