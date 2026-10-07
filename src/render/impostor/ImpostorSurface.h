#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORSURFACE_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORSURFACE_H

#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "scene/Geometry.h"

#include <cstddef>
#include <optional>
#include <string>

namespace outshine::Render {

enum class ImpostorSurfaceDetail { Flat, Depth };

[[nodiscard]] std::optional<Geometry>
BuildImpostorSurface(const Content::ImpostorAtlas &atlas,
                     size_t view,
                     ImpostorSurfaceDetail detail = ImpostorSurfaceDetail::Depth);

[[nodiscard]] std::optional<Content::ImpostorCards>
PrepareImpostorCards(const Content::ImpostorAtlas &atlas, std::string &error);

}
#endif
