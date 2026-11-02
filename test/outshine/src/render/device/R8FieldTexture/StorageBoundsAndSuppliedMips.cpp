#include "R8FieldTexture.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void SelectedRange() {
  const std::array<uint8_t, 1024> coarse{};
  const std::array<uint8_t, 256> tail{};
  const std::array mips{R8FieldMip{.Side = 32, .Pixels = coarse},
                        R8FieldMip{.Side = 16, .Pixels = tail}};
  const auto bytes = R8FieldTextureStorage(mips);
  CHECK(bytes && bytes->TexelBytes == 1024 + 256 &&
            bytes->TransferBytes == uint64_t{256} * (32 + 16),
        "only supplied coarse mips contribute texel and aligned transfer storage");
  CHECK(!R8FieldTextureStorage({}), "an empty field has no upload plan");
  const std::array nondyadic{R8FieldMip{.Side = 3, .Pixels = std::span(coarse).first(9)}};
  CHECK(!R8FieldTextureStorage(nondyadic), "a nondyadic field is rejected");
  const std::array truncated{R8FieldMip{.Side = 32, .Pixels = std::span(coarse).first(1023)}};
  CHECK(!R8FieldTextureStorage(truncated), "truncated mip pixels are rejected");
  const std::array skipped{mips[0], R8FieldMip{.Side = 8, .Pixels = std::span(tail).first(64)}};
  CHECK(!R8FieldTextureStorage(skipped), "a missing intermediate mip is rejected");
  const std::array reordered{mips[1], mips[0]};
  CHECK(!R8FieldTextureStorage(reordered), "reordered mip levels are rejected");
  const std::array<uint8_t, 1> pixel{128};
  const std::array repeated{R8FieldMip{.Side = 1, .Pixels = pixel},
                            R8FieldMip{.Side = 1, .Pixels = pixel}};
  CHECK(!R8FieldTextureStorage(repeated), "no level can follow the final one-texel mip");
  const auto last = R8FieldTextureStorage(std::span(repeated).first(1));
  CHECK(last && last->TexelBytes == 1 && last->TransferBytes == 256,
        "the final mip needs one texel and one aligned transfer row");
}

void CompleteChain() {
  const std::vector<uint8_t> pixels(size_t{512} * 512);
  std::array<R8FieldMip, 10> levels;
  for (size_t mip = 0; mip < levels.size(); ++mip) {
    const uint32_t side = 512u >> mip;
    levels[mip] = {.Side = side, .Pixels = std::span(pixels).first(size_t{side} * side)};
  }
  const auto bytes = R8FieldTextureStorage(levels);
  CHECK(bytes && bytes->TexelBytes == 349525 && bytes->TransferBytes == 392960,
        "the complete R8 chain and transient row padding are counted separately");
}
}

int main() {
  SelectedRange();
  CompleteChain();
  return Report();
}
