#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <SDL3/SDL.h>
#include "CrownAtlas.h"
#include "Image.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::ifstream input("src/assets/world/species/birch.json");
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  TreeSpecies species;
  CHECK(species.Parse(text.data(), text.size()), "the atlas uses the shipped birch generator");
  const auto tree = TreePrototype::Grow(species);
  CHECK(tree.has_value(), "the atlas has a grown source tree");
  if (!tree) { return Report(); }
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
  std::string error;
  CHECK(!CrownAtlas::Bake(*tree, {.Pixels=2,.Views=4}, error), "an atlas with no interior pixels is refused");
  const auto started = std::chrono::steady_clock::now();
  const auto atlas = CrownAtlas::Bake(*tree, {.Pixels=128,.Views=4}, error);
  CHECK(atlas.has_value(), "the native renderer captures crown surface data");
  if (!atlas) { std::printf("%s\n", error.c_str()); return Report(); }
  CHECK(atlas->Views().size() == 4 && atlas->Surfaces().size() == 2,
        "four independent views retain bark and leaf materials");
  CHECK(atlas->Surfaces()[0].Roughness == species.ShadingParams().BarkRoughness &&
        atlas->Surfaces()[1].Roughness == species.ShadingParams().LeafRoughness,
        "atlas materials retain declared roughness without baking illumination");
  std::filesystem::create_directories("build/crown-atlas");
  for (size_t view = 0; view < atlas->Views().size(); ++view) {
    const auto &texels = atlas->Views()[view].Texels;
    CHECK(texels.size() == 128u*128u, "every view has its full pixel population");
    std::array<size_t,3> covered{};
    bool valid = true;
    size_t depthMismatch = 0, badNormal = 0;
    float leastNormal = 1, mostNormal = 0;
    std::vector<uint8_t> rgba(texels.size()*4, 0);
    for (size_t at = 0; at < texels.size(); ++at) {
      const auto &pixel = texels[at];
      valid = valid && std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1;
      depthMismatch += (pixel.Depth > 0) != (pixel.Surface > 0);
      valid = valid && ((pixel.Depth > 0) == (pixel.Surface > 0));
      if (pixel.Surface == 0 || pixel.Surface > 2) { continue; }
      ++covered[pixel.Surface];
      const float lengthSquared = Dot(pixel.Normal, pixel.Normal);
      badNormal += !(std::abs(lengthSquared-1.0f) < 0.003f);
      leastNormal = std::min(leastNormal, lengthSquared);
      mostNormal = std::max(mostNormal, lengthSquared);
      valid = valid && std::abs(lengthSquared-1.0f) < 0.003f;
      const auto &colour = atlas->Surfaces()[pixel.Surface-1].BaseColour;
      for (size_t c = 0; c < 3; ++c) {
        const float linear = colour[c];
        const float srgb = linear <= 0.0031308f ? 12.92f*linear : 1.055f*std::pow(linear,1.0f/2.4f)-0.055f;
        rgba[at*4+c] = static_cast<uint8_t>(std::lround(srgb*255.0f));
      }
      rgba[at*4+3] = 255;
    }
    std::printf("view %zu depthMismatch=%zu badNormal=%zu norm2=[%g,%g]\n", view, depthMismatch, badNormal, static_cast<double>(leastNormal), static_cast<double>(mostNormal));
    CHECK(valid, "depth, coverage, surface identities and unit normals agree");
    CHECK(covered[1] > 0 && covered[2] > 0, "each crown view contains both bark and foliage");
    std::vector<uint8_t> png;
    CHECK(Core::EncodePng(rgba.data(), 128, 128, png), "intrinsic crown colour encodes as a PNG");
    std::ofstream output("build/crown-atlas/birch-"+std::to_string(view)+".png", std::ios::binary);
    output.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
    CHECK(output.good(), "crown reference PNG is written");
    std::printf("view %zu bark=%zu leaf=%zu sampled pixels\n",view,covered[1],covered[2]);
  }
  std::printf("atlas capture, checks and PNG export %.3f ms; payload %zu bytes; no world frame-rate claim\n",
              std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),
              atlas->Views().size()*128u*128u*sizeof(CrownAtlas::Texel));
  return Report();
}
