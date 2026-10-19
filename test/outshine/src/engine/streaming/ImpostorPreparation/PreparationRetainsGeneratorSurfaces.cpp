#include "Check.h"
#include "PreparationCapture.h"
#include "PreparationStorage.h"
#include "PreparationOptical.h"
#include "PreparationInstances.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <fstream>
#include <iterator>
#include <print>
#include <ratio>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Test::CrownFixture;
  std::ifstream input("src/assets/world/species/birch.json");
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  TreeSpecies species;
  CHECK(species.Parse(text.data(), text.size()), "the atlas uses the shipped birch generator");
  const auto tree = TreePrototype::Grow(species);
  CHECK(tree.has_value(), "the atlas has a grown source tree");
  if (!tree) { return Report(); }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  std::string error;
  CHECK(!BakeImpostorAtlas(*tree, {.Pixels = 2, .Views = kViews}, error),
        "an atlas with no interior pixels is refused");
  const auto began = std::chrono::steady_clock::now();
  Foreground foreground;
  if (!OpenForeground(foreground)) { return Report(); }
  Tasks worker(1);
  auto atlas = CaptureAtlas(*tree, worker, foreground);
  if (!atlas) { return Report(); }
  const auto bytes = CheckStorage(*atlas, species, text);
  if (!bytes) { return Report(); }
  const auto provenance =
      ImpostorAtlasProvenance(species.Definition(), {.Pixels = kPixels, .Views = kViews});
  if (!CheckCache(atlas, *bytes, provenance, worker) || !atlas) { return Report(); }
  CheckViews(*atlas, *tree, species);
  CheckInstances(*atlas, species, worker);
  std::println(
      "atlas capture, checks and PNG export {:.3f} ms; payload {} bytes; no world frame-rate claim",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      atlas->Views().size() * kTexels * sizeof(Content::ImpostorAtlas::Texel));
  return Report();
}
