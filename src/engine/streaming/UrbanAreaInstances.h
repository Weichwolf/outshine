#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREAINSTANCES_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREAINSTANCES_H

#include "UrbanArea.h"
#include "TangentFrame.h"
#include "Mat4.h"
#include <cstddef>
#include <expected>
#include <optional>
#include <mdspan>
#include <vector>

namespace outshine {
enum class UrbanInstanceError { InvalidRequest, MissingTerrain, OverBudget };
using UrbanPatchHeights = std::mdspan<const std::optional<double>, std::dextents<size_t, 2>>;

[[nodiscard]] std::expected<std::vector<Mat4>, UrbanInstanceError>
PrepareUrbanPatchInstances(const Generators::Osm::UrbanArea &area,
                           Generators::Osm::UrbanPatchRange patch,
                           UrbanPatchHeights groundAslM,
                           const TangentFrame &frame,
                           size_t instancesMost);
}
#endif
