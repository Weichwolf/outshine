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

// Fractional coverage provides colour support, never opaque depth or shadow depth.
[[nodiscard]] std::optional<Geometry>
BuildFractionalImpostorSurface(const ImpostorAtlas &atlas, size_t view, size_t firstMip = 0);

[[nodiscard]] std::optional<ImpostorCards> PrepareImpostorCards(const ImpostorAtlas &atlas,
                                                                std::string &error);

}
#endif
