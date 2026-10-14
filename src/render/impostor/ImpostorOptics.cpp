#include "ImpostorOptics.h"
#include "shade/TexelChain.h"
#include "math/Srgb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

namespace outshine::Render {
namespace {
constexpr size_t kOpticalImages = 5;
constexpr float kByteSteps = 255;
using OpticalFields = std::array<std::vector<float>, kOpticalImages>;
using OpticalImages = std::array<std::vector<uint8_t>, kOpticalImages>;

uint8_t Byte(float value) {
  return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * kByteSteps));
}

OpticalFields FieldsOf(const Content::ImpostorAtlas &atlas,
                       const Content::ImpostorAtlas::View &source) {
  OpticalFields fields;
  for (auto &field : fields) { field.resize(source.Texels.size() * 4); }
  const Vec3 right{{source.TowardEye[2], 0, -source.TowardEye[0]}};
  for (size_t at = 0; at < source.Texels.size(); ++at) {
    const auto &pixel = source.Texels[at];
    if (pixel.Surface == 0) { continue; }
    const auto &row = atlas.Surfaces()[pixel.Surface - 1];
    const auto sample =
        source.Materials.empty()
            ? Content::ImpostorAtlas::SurfaceSample{.BaseColour = {{row.BaseColour[0],
                                                                    row.BaseColour[1],
                                                                    row.BaseColour[2]}},
                                                    .Roughness = row.Roughness,
                                                    .Metalness = row.Metalness}
            : source.Materials[at];
    Vec3 n{{pixel.Normal[0], pixel.Normal[1], pixel.Normal[2]}};
    if (!Normalise(n)) { return {}; }
    const size_t offset = at * 4;
    fields[0][offset + 3] = fields[1][offset + 3] = 1;
    fields[1][offset] = static_cast<float>(Dot(n, right));
    fields[1][offset + 1] = static_cast<float>(-n[1]);
    fields[1][offset + 2] = static_cast<float>(Dot(n, source.TowardEye));
    if (row.Scattering == ScatteringModel::DiffuseThinSheet) {
      for (size_t channel = 0; channel < 3; ++channel) {
        fields[3][offset + channel] = sample.BaseColour[channel];
        fields[4][offset + channel] = row.DiffuseTransmission[channel];
      }
      fields[3][offset + 3] = 1;
    } else {
      for (size_t channel = 0; channel < 3; ++channel) {
        fields[0][offset + channel] = sample.BaseColour[channel];
      }
      const float alpha = sample.Roughness * sample.Roughness;
      fields[2][offset + 1] = alpha * alpha;
      fields[2][offset + 2] = sample.Metalness;
      fields[2][offset + 3] = 1;
    }
  }
  return fields;
}

std::array<float, 4> NormalTexel(const std::vector<float> &field, size_t offset) {
  const float coverage = field[offset + 3];
  Vec3 n{{field[offset], field[offset + 1], field[offset + 2]}};
  const double length = Length(n);
  if (length > 0) {
    n = n * (1.0 / length);
  } else {
    n = {{0, 0, 1}};
  }
  std::array<float, 4> result{};
  for (size_t channel = 0; channel < 3; ++channel) {
    result[channel] = static_cast<float>(0.5 * (n[channel] + 1));
  }
  result[3] = coverage > 0 ? static_cast<float>(length / coverage) : 1;
  return result;
}

OpticalImages Encoded(const OpticalFields &fields) {
  OpticalImages images;
  for (auto &image : images) { image.resize(fields[0].size()); }
  for (size_t offset = 0; offset < fields[0].size(); offset += 4) {
    const auto normal = NormalTexel(fields[1], offset);
    for (size_t image = 0; image < images.size(); ++image) {
      for (size_t channel = 0; channel < 4; ++channel) {
        float value = image == 1 ? normal[channel] : fields[image][offset + channel];
        if (image == 0 && channel < 3) { value = ColourSpace::SrgbFromLinear(value); }
        images[image][offset + channel] = Byte(value);
      }
    }
  }
  return images;
}
}

bool AddOpticalCardImages(Geometry &geometry,
                          Material &material,
                          const Content::ImpostorAtlas &atlas,
                          const Content::ImpostorAtlas::View &source) {
  if (std::ranges::none_of(source.Texels, [](const auto &pixel) { return pixel.Surface > 0; })) {
    return false;
  }
  auto fields = FieldsOf(atlas, source);
  if (fields[0].empty()) { return false; }
  const auto base = Encoded(fields);
  OpticalImages lower;
  Core::Texels extent{.WidthPx = static_cast<uint32_t>(atlas.Pixels()),
                      .HeightPx = static_cast<uint32_t>(atlas.Pixels())};
  while (extent.WidthPx > 1 || extent.HeightPx > 1) {
    Core::Texels smaller;
    for (auto &field : fields) {
      std::vector<float> filtered;
      smaller = Core::HalveInPlace(field, extent, filtered, Core::TexelKind::Value);
      field = std::move(filtered);
    }
    extent = smaller;
    const auto level = Encoded(fields);
    for (size_t at = 0; at < lower.size(); ++at) {
      lower[at].insert(lower[at].end(), level[at].begin(), level[at].end());
    }
  }
  material.Scattering = ScatteringModel::AreaMixture;
  material.BaseColour = {{1, 1, 1, 1}};
  material.DiffuseTransmission = {{1, 1, 1}};
  material.Metalness = material.Roughness = 1;
  material.Alpha = AlphaMode::Masked;
  const std::array bindings{std::pair{&material.BaseColourMap, ImageMipKind::Colour},
                            std::pair{&material.NormalMap, ImageMipKind::Normal},
                            std::pair{&material.MetalRoughMap, ImageMipKind::Linear},
                            std::pair{&material.DiffuseReflectionMap, ImageMipKind::Linear},
                            std::pair{&material.DiffuseTransmissionMap, ImageMipKind::Linear}};
  for (size_t at = 0; at < bindings.size(); ++at) {
    const auto [map, kind] = bindings[at];
    ImageMipData mips;
    mips[static_cast<size_t>(kind)] = std::move(lower[at]);
    const auto image = geometry.addImage({.WidthPx = atlas.Pixels(),
                                          .HeightPx = atlas.Pixels(),
                                          .Rgba = base[at],
                                          .LowerMips = ViewImageMips(mips)});
    if (!image) { return false; }
    map->Image = *image;
    map->Sampler.WrapU = map->Sampler.WrapV = Wrap::ClampToEdge;
  }
  return true;
}
}
