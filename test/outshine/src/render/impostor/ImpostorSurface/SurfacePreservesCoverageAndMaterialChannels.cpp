#include "ImpostorSurface.h"

#include "Check.h"
#include "math/Srgb.h"
#include "shade/HalfTexels.h"

#include <string>
#include <array>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace {
constexpr outshine::Vec4f kWoodColour{{.3f, .14f, .05f, 1}};
constexpr float kWoodRoughness = .7f;
constexpr outshine::Vec4f kLeafColour{{.1f, .2f, .03f, 1}};
constexpr outshine::Vec3f kLeafTransmission{{.4f, .6f, .1f}};
constexpr outshine::Vec4f kOpaqueColour{{.25f, .5f, .75f, 1}};
constexpr float kOpaqueMetalness = .125f;
constexpr float kQuarter = .25f;
constexpr float kThreeQuarters = .75f;
constexpr float kByteSteps = 255;
constexpr double kColourBound = .003;
constexpr double kCoefficientBound = .0021;
constexpr size_t kOpticalTexels = 16 + 4 + 1;
constexpr size_t kOpticalTexelBytes = 24;
constexpr size_t kSparseMapBytes = size_t{3} * 3 * 4;
constexpr size_t kHalfMipBytes = (size_t{4} + 1) * 8;

void CheckOpticalStorage(const outshine::Geometry &card, float roughness) {
  using namespace outshine;
  using namespace outshine::Test;
  size_t storedBytes = 0;
  for (int at = 0; at < card.images(); ++at) {
    const auto image = card.imageAt(at);
    CHECK(image.Format == (at == 2 ? ImagePixelFormat::Rgba16Float : ImagePixelFormat::Rgba8Unorm),
          "only the optical material-moment map uses half-float storage");
    storedBytes += image.Rgba.size();
    for (const auto &mip : image.LowerMips) {
      if (mip) { storedBytes += mip->size(); }
    }
  }
  CHECK(storedBytes == kOpticalTexels * kOpticalTexelBytes,
        "five jointly filtered optical maps consume exactly24 bytes per mip texel");
  const auto momentImage = card.imageAt(2);
  const auto momentMips = momentImage.LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  CHECK(momentMips && momentMips->size() == kHalfMipBytes,
        "half-float moment mip extents are retained explicitly");
  if (!momentMips || momentMips->size() != kHalfMipBytes) { return; }
  const auto moment = Core::UnpackHalfTexels({.WidthPx = 2, .HeightPx = 2}, *momentMips);
  CHECK(moment && (*moment)[3] == kQuarter,
        "opaque coverage keeps its premultiplied area through the first half-float mip");
  if (!moment) { return; }
  constexpr float kHalfStep = 0x1p-10f;
  const float alpha = roughness * roughness;
  CHECK_NEAR((*moment)[1],
             alpha * alpha * kQuarter,
             alpha * alpha * kQuarter * kHalfStep,
             "linear moment",
             "roughness fourth moments retain the same spatial filtering");
}

void CheckOpticalFiltering() {
  using namespace outshine;
  using namespace outshine::Test;
  Material wood;
  wood.BaseColour = kWoodColour;
  wood.Roughness = kWoodRoughness;
  Material leaf;
  leaf.Scattering = ScatteringModel::DiffuseThinSheet;
  leaf.BaseColour = kLeafColour;
  leaf.DiffuseTransmission = kLeafTransmission;
  Content::ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(16);
  view.Texels[0] = {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1};
  view.Texels[1] = {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 2};
  std::string error;
  const auto atlas = Content::ImpostorAtlas::Create(4, {}, 1, {wood, leaf}, {view}, error);
  CHECK(atlas.has_value(), "opaque and transmissive source coefficients share a native atlas");
  if (!atlas) { return; }
  const auto encoded = atlas->Encode("optical-fixture", error);
  CHECK(encoded && (*encoded)[8] == 3, "uncomposed optical surfaces use their own binary layout");
  const auto decoded =
      encoded ? Content::ImpostorAtlas::Decode(*encoded, "optical-fixture", error) : std::nullopt;
  CHECK(decoded && decoded->Surfaces()[1] == leaf,
        "optical source material survives cache replay exactly");
  const auto card = Render::BuildImpostorSurface(*atlas, 0, Render::ImpostorSurfaceDetail::Flat);
  CHECK(card && card->images() == 5 && card->wellFormed(),
        "one mixed card owns all five jointly filtered maps");
  if (!card || card->images() != 5 || !card->wellFormed()) { return; }
  CheckOpticalStorage(*card, wood.Roughness);
  const auto &row = card->surfaceAt(MaterialInstance(0));
  CHECK(row.Scattering == ScatteringModel::AreaMixture && row.DiffuseReflectionMap.Image == 3 &&
            row.DiffuseTransmissionMap.Image == 4,
        "the native card retains both source response models");
  const auto colourMips = card->imageAt(0).LowerMips[static_cast<size_t>(ImageMipKind::Colour)];
  const auto normalMips = card->imageAt(1).LowerMips[static_cast<size_t>(ImageMipKind::Normal)];
  const auto reflectionMips = card->imageAt(3).LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  const auto transmissionMips =
      card->imageAt(4).LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  const bool complete = colourMips && normalMips && reflectionMips && transmissionMips;
  CHECK(complete, "all jointly filtered optical images retain their prepared mips");
  if (!colourMips || !normalMips || !reflectionMips || !transmissionMips) { return; }
  const bool haveCoefficients =
      std::ranges::all_of(std::array{*colourMips, *normalMips, *reflectionMips, *transmissionMips},
                          [](const auto levels) { return levels.size() >= 4; });
  CHECK(haveCoefficients, "prepared optical images contain all four first-mip coefficients");
  if (!haveCoefficients) { return; }
  const auto colour = *colourMips;
  const auto normal = *normalMips;
  const auto reflection = *reflectionMips;
  const auto transmission = *transmissionMips;
  CHECK(colour[3] == 128 && reflection[3] == 64,
        "the first mip retains total and sheet coverage independently");
  CHECK(normal[0] == 128 && normal[1] == 128 && normal[2] == 255 && normal[3] == 255,
        "empty texels do not broaden a constant covered normal");
  for (size_t channel = 0; channel < 3; ++channel) {
    CHECK_NEAR(ColourSpace::LinearFromSrgb(colour[channel] / kByteSteps),
               wood.BaseColour[channel] * kQuarter,
               kColourBound,
               "linear coefficient",
               "wood colour remains coverage-premultiplied before sampling");
    CHECK_NEAR(reflection[channel] / kByteSteps,
               leaf.BaseColour[channel] * kQuarter,
               kCoefficientBound,
               "linear coefficient",
               "sheet reflection is coverage weighted without empty-texel padding");
    CHECK_NEAR(transmission[channel] / kByteSteps,
               leaf.DiffuseTransmission[channel] * kQuarter,
               kCoefficientBound,
               "linear coefficient",
               "sheet transmission shares the same area-filtered footprint");
  }
}

void CheckDepthSurface(const outshine::Material &surface) {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  std::vector<Content::ImpostorAtlas::Texel> stepped(16);
  for (size_t pixel = 0; pixel < stepped.size(); ++pixel) {
    stepped[pixel] = {
        .Normal = {{0, 0, 1}}, .Depth = pixel % 4 < 2 ? kQuarter : kThreeQuarters, .Surface = 1};
  }
  const auto depthAtlas = Content::ImpostorAtlas::Create(
      4,
      {},
      2.0,
      {surface},
      {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(stepped), .Materials = {}}},
      error);
  CHECK(depthAtlas.has_value(), "two separated surfaces form a valid capture");
  if (depthAtlas) {
    const auto depthCard = Render::BuildImpostorSurface(*depthAtlas, 0);
    CHECK(depthCard && depthCard->wellFormed(), "captured depth reaches native geometry");
    if (depthCard) {
      bool near = false;
      bool far = false;
      const auto positions = depthCard->positionsOf(0);
      for (size_t at = 2; at < positions.size(); at += 3) {
        near = near || positions[at] == 2.0f;
        far = far || positions[at] == -2.0f;
        CHECK(positions[at] == 2.0f || positions[at] == -2.0f,
              "no flat carrier or bridge replaces the separated depths");
      }
      CHECK(near && far, "both reconstructed surface depths remain available");
      CHECK(depthCard->trianglesOf(0).size() == 24,
            "four planar regions need eight triangles instead of one quad per texel");
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Material surface;
  surface.BaseColour = kOpaqueColour;
  surface.Metalness = kOpaqueMetalness;
  surface.Roughness = 0.5f;
  std::vector<Content::ImpostorAtlas::Texel> texels(9);
  texels[4] = {.Normal = {{0, 0, 1}}, .Depth = 0.5f, .Surface = 1};
  std::string error;
  auto atlas = Content::ImpostorAtlas::Create(
      3,
      {},
      1.0,
      {surface},
      {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(texels), .Materials = {}}},
      error);
  CHECK(atlas.has_value(), "valid captured samples form an atlas");
  if (!atlas) { return Report(); }

  const auto card = Render::BuildImpostorSurface(*atlas, 0);
  CHECK(card && card->wellFormed() && card->images() == 3,
        "one view produces a complete native card and three material maps");
  CHECK(!Render::BuildImpostorSurface(*atlas, 1), "an unavailable view is rejected");
  if (!card || card->images() != 3) { return Report(); }

  const std::array kinds{ImageMipKind::Colour, ImageMipKind::Normal, ImageMipKind::Linear};
  for (size_t at = 0; at < kinds.size(); ++at) {
    const auto image = card->imageAt(static_cast<int>(at));
    const auto lower = image.LowerMips[static_cast<size_t>(kinds[at])];
    CHECK(image.valid() && lower && lower->size() == 4,
          "each odd-sized card map carries its complete interpretation-specific lower chain");
  }

  const auto colour = card->imageAt(0).Rgba;
  const auto normal = card->imageAt(1).Rgba;
  const auto metalRough = card->imageAt(2).Rgba;
  CHECK(colour.size() == kSparseMapBytes && normal.size() == kSparseMapBytes &&
            metalRough.size() == kSparseMapBytes,
        "all maps cover the declared atlas dimensions");
  if (colour.size() != kSparseMapBytes || normal.size() != kSparseMapBytes ||
      metalRough.size() != kSparseMapBytes) {
    return Report();
  }
  for (size_t pixel = 0; pixel < 9; ++pixel) {
    const size_t at = pixel * 4;
    CHECK(colour[at] == 137 && colour[at + 1] == 188 && colour[at + 2] == 225 &&
              colour[at + 3] == (pixel == 4 ? 255 : 0),
          "nearest colour padding does not expand alpha coverage");
    CHECK(normal[at] == 128 && normal[at + 1] == 128 && normal[at + 2] == 255,
          "world normals enter the billboard tangent frame");
    CHECK(metalRough[at + 1] == 128 && metalRough[at + 2] == 32,
          "roughness and metalness retain independent channels");
  }
  CheckDepthSurface(surface);
  CheckOpticalFiltering();
  return Report();
}
