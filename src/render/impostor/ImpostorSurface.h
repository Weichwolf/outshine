#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORSURFACE_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORSURFACE_H

#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "Geometry.h"

#include <cstddef>
#include <optional>
#include <string>

namespace outshine::Render {

enum class ImpostorSurfaceDetail { Flat, Depth };

[[nodiscard]] std::optional<Geometry>
BuildImpostorSurface(const ImpostorAtlas &atlas,
                     size_t view,
                     ImpostorSurfaceDetail detail = ImpostorSurfaceDetail::Depth);

[[nodiscard]] std::optional<ImpostorCards> PrepareImpostorCards(const ImpostorAtlas &atlas,
                                                                std::string &error);

}
#endif
