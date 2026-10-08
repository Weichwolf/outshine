#ifndef OUTSHINE_CONTENT_SHADE_IMAGEMIPS_H
#define OUTSHINE_CONTENT_SHADE_IMAGEMIPS_H

#include "scene/Texture.h"
#include <optional>
#include <vector>

namespace outshine::Core {
[[nodiscard]] std::optional<std::vector<uint8_t>> PrepareImageMips(ImageView image,
                                                                   ImageMipKind kind);
}
#endif
