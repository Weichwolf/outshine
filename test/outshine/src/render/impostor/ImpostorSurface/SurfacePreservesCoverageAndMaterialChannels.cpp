#include "ImpostorSurface.h"

#include "Check.h"
#include "math/Srgb.h"

#include <string>
#include <array>
#include <vector>

namespace {
void CheckOpticalFiltering() {
  using namespace outshine;
  using namespace outshine::Test;
  Material wood;
  wood.BaseColour = {{.3f, .14f, .05f, 1}};
  wood.Roughness = .7f;
  Material leaf;
  leaf.Scattering = ScatteringModel::DiffuseThinSheet;
  leaf.BaseColour = {{.1f, .2f, .03f, 1}};
  leaf.DiffuseTransmission = {{.4f, .6f, .1f}};
  Content::ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}};
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
  if (!card) { return; }
  const auto &row = card->surfaceAt(MaterialInstance(0));
  CHECK(row.Scattering == ScatteringModel::AreaMixture && row.DiffuseReflectionMap.Image == 3 &&
            row.DiffuseTransmissionMap.Image == 4,
        "the native card retains both source response models");
  const auto colour = *card->imageAt(0).LowerMips[static_cast<size_t>(ImageMipKind::Colour)];
  const auto normal = *card->imageAt(1).LowerMips[static_cast<size_t>(ImageMipKind::Normal)];
  const auto reflection = *card->imageAt(3).LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  const auto transmission = *card->imageAt(4).LowerMips[static_cast<size_t>(ImageMipKind::Linear)];
  CHECK(colour[3] == 128 && reflection[3] == 64,
        "the first mip retains total and sheet coverage independently");
  CHECK(normal[0] == 128 && normal[1] == 128 && normal[2] == 255 && normal[3] == 255,
        "empty texels do not broaden a constant covered normal");
  for (size_t channel = 0; channel < 3; ++channel) {
    CHECK_NEAR(ColourSpace::LinearFromSrgb(colour[channel] / 255.0f),
               wood.BaseColour[channel] * .25f,
               .003,
               "linear coefficient",
               "wood colour remains coverage-premultiplied before sampling");
    CHECK_NEAR(reflection[channel] / 255.0f,
               leaf.BaseColour[channel] * .25f,
               .0021,
               "linear coefficient",
               "sheet reflection is coverage weighted without empty-texel padding");
    CHECK_NEAR(transmission[channel] / 255.0f,
               leaf.DiffuseTransmission[channel] * .25f,
               .0021,
               "linear coefficient",
               "sheet transmission shares the same area-filtered footprint");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Material surface;
  surface.BaseColour = {{0.25f, 0.5f, 0.75f, 1.0f}};
  surface.Metalness = 0.125f;
  surface.Roughness = 0.5f;
  std::vector<Content::ImpostorAtlas::Texel> texels(9);
  texels[4] = {.Normal = {{0, 0, 1}}, .Depth = 0.5f, .Surface = 1};
  std::string error;
  auto atlas = Content::ImpostorAtlas::Create(
      3, {}, 1.0, {surface}, {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(texels)}}, error);
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
  CHECK(colour.size() == 36 && normal.size() == 36 && metalRough.size() == 36,
        "all maps cover the declared atlas dimensions");
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
  std::vector<Content::ImpostorAtlas::Texel> stepped(16);
  for (size_t pixel = 0; pixel < stepped.size(); ++pixel) {
    stepped[pixel] = {.Normal = {{0, 0, 1}}, .Depth = pixel % 4 < 2 ? 0.25f : 0.75f, .Surface = 1};
  }
  const auto depthAtlas = Content::ImpostorAtlas::Create(
      4, {}, 2.0, {surface}, {{.TowardEye = {{0, 0, 1}}, .Texels = std::move(stepped)}}, error);
  CHECK(depthAtlas.has_value(), "two separated surfaces form a valid capture");
  if (depthAtlas) {
    const auto depthCard = Render::BuildImpostorSurface(*depthAtlas, 0);
    CHECK(depthCard && depthCard->wellFormed(), "captured depth reaches native geometry");
    if (depthCard) {
      bool near = false, far = false;
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
  CheckOpticalFiltering();
  return Report();
}
