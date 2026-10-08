#include "ImpostorSurface.h"

#include "Check.h"

#include <string>
#include <array>
#include <vector>

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
  return Report();
}
