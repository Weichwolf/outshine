#include "ImpostorAtlas.h"
#include "Check.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {
void Resign(std::vector<uint8_t> &bytes) {
  uint64_t hash = 14695981039346656037ull;
  for (size_t at = 0; at < bytes.size() - 8; ++at) { hash = (hash ^ bytes[at]) * 1099511628211ull; }
  for (size_t at = 0; at < 8; ++at) {
    bytes[bytes.size() - 8 + at] = static_cast<uint8_t>(hash >> (8 * at));
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Content::ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(9);
  view.Materials.resize(9);
  view.Texels[4] = {.Normal = {{0, 0, 1}}, .Depth = 0.5f, .Surface = 1};
  view.Materials[4] = {
      .BaseColour = {{0.125f, 0.25f, 0.75f}}, .Roughness = 0.5f, .Metalness = 0.25f};
  std::string error;
  auto atlas = Content::ImpostorAtlas::Create(3, {}, 1, {Material{}}, {view}, error);
  CHECK(atlas.has_value(), "composed material samples form a valid atlas");
  if (!atlas) { return Report(); }
  const auto encoded = atlas->Encode("composed-fixture", error);
  CHECK(encoded && encoded->size() == 508 && (*encoded)[8] == 2,
        "v2 adds exactly five float fields per pixel to the independent v1 layout");
  if (!encoded) { return Report(); }
  const auto decoded = Content::ImpostorAtlas::Decode(*encoded, "composed-fixture", error);
  CHECK(decoded && decoded->Views()[0].Materials.size() == 9,
        "composed values survive asset transport");
  if (!decoded) { return Report(); }
  const auto &sample = decoded->Views()[0].Materials[4];
  CHECK(sample.BaseColour == view.Materials[4].BaseColour && sample.Roughness == 0.5f &&
            sample.Metalness == 0.25f && decoded->Encode("composed-fixture", error) == encoded,
        "colour and both material channels survive exactly with canonical bytes");
  auto corrupt = *encoded;
  // Fourth foreground sample: 64-byte header, 52-byte material, 24-byte view,
  // nine 20-byte geometry texels; RGB occupies the first twelve bytes of each sample.
  const size_t roughnessAt = 64 + 52 + 24 + 9 * 20 + 4 * 20 + 12;
  corrupt[roughnessAt] = 0;
  corrupt[roughnessAt + 1] = 0;
  corrupt[roughnessAt + 2] = 128;
  corrupt[roughnessAt + 3] = 127;
  Resign(corrupt);
  CHECK(!Content::ImpostorAtlas::Decode(corrupt, "composed-fixture", error),
        "nonfinite composed values are rejected even with a valid checksum");
  view.Materials.pop_back();
  CHECK(!Content::ImpostorAtlas::Create(3, {}, 1, {Material{}}, {view}, error),
        "partial material coverage is rejected");
  view.Materials.resize(9);
  view.Materials[4].Metalness = 1.1f;
  CHECK(!Content::ImpostorAtlas::Create(3, {}, 1, {Material{}}, {view}, error),
        "unrepresentable material channels cannot be silently clamped");
  return Report();
}
