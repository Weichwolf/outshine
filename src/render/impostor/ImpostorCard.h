#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORCARD_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORCARD_H

#include "ImpostorAtlas.h"
#include "scene/Geometry.h"

#include <cstddef>
#include <optional>

namespace outshine::Render {

[[nodiscard]] std::optional<Geometry> BuildImpostorCard(const Content::ImpostorAtlas &atlas,
                                                        size_t view);

}
#endif
