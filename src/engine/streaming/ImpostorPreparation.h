#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTORPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTORPREPARATION_H

#include "ImpostorAtlas.h"
#include "ImpostorAtlasShape.h"
#include "TreePrototype.h"
#include <optional>
#include <string>
#include <string_view>

namespace outshine {
namespace Render {
class ImpostorBaker;
}

[[nodiscard]] std::optional<ImpostorAtlas> BakeImpostorAtlas(Render::ImpostorBaker &baker,
                                                             const Generators::TreePrototype &tree,
                                                             ImpostorAtlasShape shape,
                                                             std::string &error);

[[nodiscard]] std::optional<ImpostorAtlas> BakeImpostorAtlas(const Generators::TreePrototype &tree,
                                                             ImpostorAtlasShape shape,
                                                             std::string &error);

[[nodiscard]] std::string ImpostorAtlasProvenance(std::string_view species,
                                                  ImpostorAtlasShape shape);

}
#endif
