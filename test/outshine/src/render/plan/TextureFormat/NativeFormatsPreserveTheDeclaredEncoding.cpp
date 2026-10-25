#include "TextureFormat.h"
#include "Check.h"
#include <array>
#include <utility>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr std::array encodings{
      std::pair{TexelFormat::Rgba16Float, Native::Format::Rgba16Float},
      std::pair{TexelFormat::Rgba32Float, Native::Format::Rgba32Float},
      std::pair{TexelFormat::Rg16Float, Native::Format::Rg16Float},
      std::pair{TexelFormat::R8Unorm, Native::Format::R8Unorm},
      std::pair{TexelFormat::Rgba8UnormSrgb, Native::Format::Rgba8Srgb},
      std::pair{TexelFormat::Depth32Float, Native::Format::Depth32Float}};
  for (const auto &[declared, native] : encodings) {
    CHECK(TextureFormatOf(declared) == native,
          "native image format preserves precision, components and transfer encoding");
  }
  CHECK(TextureFormatOf(TexelFormat::Handle) == Native::Format::Count &&
            TextureFormatOf(TexelFormat::Table) == Native::Format::Count,
        "logical handles and tables do not masquerade as texture formats");
  return Report();
}
