#ifndef OUTSHINE_TEST_RENDER_STOREDSAMPLEQUADS_H
#define OUTSHINE_TEST_RENDER_STOREDSAMPLEQUADS_H

#include "Geometry.h"
#include "Viewing.h"
#include <cstddef>
#include <expected>
#include <string>

namespace outshine {
class ImpostorAtlas;
}

namespace outshine::Test {
struct StoredSampleQuads {
  struct Request {
    size_t View = 0;
    size_t SourceBytesMost = 0;
  };

  Geometry Source;
  Render::Viewpoint Capture;
  // Active image/mesh payload; allocator, preparation and GPU peaks remain separate.
  size_t SourceBytes = 0;

  [[nodiscard]] static std::expected<StoredSampleQuads, std::string>
  Prepare(const ImpostorAtlas &atlas, Request request);
};
}
#endif
