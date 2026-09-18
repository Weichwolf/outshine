#include "ImpostorCard.h"

#include "Check.h"

#include <string>
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

  const auto card = Render::BuildImpostorCard(*atlas, 0);
  CHECK(card && card->wellFormed() && card->images() == 3,
        "one view produces a complete native card and three material maps");
  CHECK(!Render::BuildImpostorCard(*atlas, 1), "an unavailable view is rejected");
  if (!card || card->images() != 3) { return Report(); }

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
  return Report();
}
