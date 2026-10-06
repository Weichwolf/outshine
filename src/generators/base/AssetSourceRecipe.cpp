#include "AssetSourceRecipe.h"
#include "SourceSet.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <span>

namespace outshine::Generators {
std::string AssetSourceRecipe(std::string_view version,
                              const Data::SourceSet &sources,
                              std::span<const Data::DataKind> kinds) {
  std::string recipe(version);
  for (size_t at = 0; at < sources.Count(); ++at) {
    const auto &decl = sources.At(at).Declaration();
    if (std::ranges::find(kinds, decl.Kind) == kinds.end()) { continue; }
    recipe += Data::ContentKey(decl, Data::Address::Whole(0));
    for (const auto value : {static_cast<int>(decl.Wire),
                             static_cast<int>(decl.How),
                             static_cast<int>(decl.Order),
                             static_cast<int>(decl.OnAbsent),
                             static_cast<int>(decl.TileAbsence),
                             decl.MinZoom,
                             decl.MaxZoom,
                             static_cast<int>(decl.AncestorFill)}) {
      recipe += ':';
      recipe += std::to_string(value);
    }
    recipe += ':';
    recipe += decl.Schema;
  }
  return Sha256Hex(recipe);
}
}
