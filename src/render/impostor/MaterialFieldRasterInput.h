#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDRASTERINPUT_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDRASTERINPUT_H
#include "Viewing.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace outshine {
class ImpostorAtlas;
}

namespace outshine::Render {
struct MaterialFieldRasterInput {
  struct Request {
    size_t View = 0;
    size_t BytesMost = 0;
  };

  struct alignas(16) Word {
    std::array<uint32_t, 4> Values{};
  };

  Viewpoint Capture;
  std::vector<Word> Words;
  [[nodiscard]] static std::expected<MaterialFieldRasterInput, std::string>
  Prepare(const ImpostorAtlas &atlas, Request request);
};

static_assert(sizeof(MaterialFieldRasterInput::Word) == 16);
static_assert(alignof(MaterialFieldRasterInput::Word) == 16);
static_assert(offsetof(MaterialFieldRasterInput::Word, Values) == 0);
}
#endif
