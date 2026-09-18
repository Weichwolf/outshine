#include "ImpostorAtlas.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {
// Independent little-endian v1 fixture: 3x3, one material, one horizontal view,
// one foreground texel. FNV-1a identifies "codec-fixture" and checks the bytes.
constexpr std::array<uint8_t, 328> kFixture{
    {79,  83, 67, 82, 79, 87, 78, 0, 1,  0,  0,   0,  249, 248, 168, 204, 131, 175, 119, 35, 3, 0,
     0,   0,  1,  0,  0,  0,  1,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   240, 63, 0, 0,
     128, 62, 0,  0,  0,  63, 0,  0, 64, 63, 0,   0,  128, 63,  0,   0,   0,   62,  0,   0,  0, 63,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   63,  0,   0,   0,   0,  1, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  240, 63,  0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  128, 63, 0,   0,   0,   63,  1,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  0,   0,   0,   0,   0,   0,   0,   0,  0, 0,
     0,   0,  0,  0,  0,  0,  0,  0, 0,  0,  0,   0,  240, 74,  8,   247, 184, 9,   152, 82}};

void Put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) { bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i)); }
}

void Resign(std::vector<uint8_t> &bytes) {
  uint64_t checksum = 14695981039346656037ull;
  for (size_t i = 0; i < bytes.size() - 8; ++i) {
    checksum = (checksum ^ bytes[i]) * 1099511628211ull;
  }
  for (size_t i = 0; i < 8; ++i) {
    bytes[bytes.size() - 8 + i] = static_cast<uint8_t>(checksum >> (8 * i));
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  const auto atlas = Content::ImpostorAtlas::Decode(kFixture, "codec-fixture", error);
  CHECK(atlas.has_value(), "hand-encoded native artifact is readable without renderer setup");
  if (!atlas) { return Report(); }
  CHECK(atlas->Pixels() == 3 && atlas->Views().size() == 1 && atlas->Surfaces().size() == 1,
        "declared dimensions and tables preserved");
  CHECK(atlas->Surfaces()[0].BaseColour[2] == .75f && atlas->Surfaces()[0].Metalness == .125f &&
            atlas->Surfaces()[0].DoubleSided,
        "independent material fields decoded");
  CHECK(atlas->Views()[0].Texels[4].Surface == 1 && atlas->Views()[0].Texels[4].Depth == .5f &&
            atlas->Views()[0].Texels[4].Normal[2] == 1.0f,
        "foreground depth normal and one-based material identity preserved");
  const auto encoded = atlas->Encode("codec-fixture", error);
  CHECK(encoded && *encoded == std::vector<uint8_t>(kFixture.begin(), kFixture.end()),
        "encoder matches independently specified complete binary layout");
  CHECK(!Content::ImpostorAtlas::Decode(kFixture, "other-source", error),
        "provenance mismatch rejected");
  for (const size_t length : {size_t{0}, size_t{64}, kFixture.size() - 1}) {
    CHECK(
        !Content::ImpostorAtlas::Decode(std::span{kFixture}.first(length), "codec-fixture", error),
        "truncation rejected before field access");
  }

  struct Invalid {
    size_t Offset;
    uint32_t Value;
  };

  for (const auto mutation : std::array<Invalid, 9>{{{20, 2},
                                                     {24, 0xffffffffu},
                                                     {84, 0x7fc00000u},
                                                     {104, 3},
                                                     {108, 2},
                                                     {140, 0x3f800000u},
                                                     {228, 0},
                                                     {232, 0},
                                                     {236, 2}}}) {
    std::vector<uint8_t> changed(kFixture.begin(), kFixture.end());
    Put32(changed, mutation.Offset, mutation.Value);
    Resign(changed);
    CHECK(!Content::ImpostorAtlas::Decode(changed, "codec-fixture", error),
          "invalid dimensions materials or texels rejected despite a valid checksum");
  }
  std::vector<uint8_t> direction(kFixture.begin(), kFixture.end());
  Put32(direction, 120, 0);
  Resign(direction);
  CHECK(!Content::ImpostorAtlas::Decode(direction, "codec-fixture", error),
        "nonunit view direction rejected with a valid checksum");
  return Report();
}
