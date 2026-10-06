#ifndef OUTSHINE_GENERATORS_BASE_ASSETSOURCERECIPE_H
#define OUTSHINE_GENERATORS_BASE_ASSETSOURCERECIPE_H

#include <world/data/DataKind.h>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {
[[nodiscard]] std::string AssetSourceRecipe(std::string_view version,
                                            const Data::SourceSet &sources,
                                            std::span<const Data::DataKind> kinds);
}
#endif
