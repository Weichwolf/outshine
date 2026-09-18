#ifndef OUTSHINE_ENGINE_IMPOSTORPREPARATION_H
#define OUTSHINE_ENGINE_IMPOSTORPREPARATION_H

#include "ImpostorAtlas.h"
#include "ImpostorAtlasShape.h"
#include "TreePrototype.h"
#include <optional>
#include <cstddef>
#include <string>
#include <string_view>

namespace outshine {

[[nodiscard]] std::optional<Content::ImpostorAtlas> BakeImpostorAtlas(
    const Generators::TreePrototype &tree, Content::ImpostorAtlasShape shape, std::string &error);

[[nodiscard]] std::string ImpostorAtlasProvenance(std::string_view species,
                                                  Content::ImpostorAtlasShape shape);

[[nodiscard]] std::optional<Geometry> BuildImpostorCard(const Content::ImpostorAtlas &atlas,
                                                        size_t view);

}
#endif
