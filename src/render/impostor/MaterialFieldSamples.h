#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDSAMPLES_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDSAMPLES_H

#include "Geometry.h"
#include "Viewing.h"
#include <cstddef>
#include <expected>
#include <string>

namespace outshine {
class ImpostorAtlas;
}

namespace outshine::Render {
struct MaterialFieldSamples {
  struct Request {
    size_t View = 0;
    size_t SourceBytesMost = 0;
  };

  Geometry Source;
  Viewpoint Capture;
  // Active image/mesh payload; allocator, preparation and GPU peaks remain separate.
  size_t SourceBytes = 0;

  [[nodiscard]] static std::expected<MaterialFieldSamples, std::string>
  Prepare(const ImpostorAtlas &atlas, Request request);
};
}
#endif
