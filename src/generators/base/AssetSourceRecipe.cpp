#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "SourceSet.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <span>

namespace outshine::Generators {
namespace {
bool WriteSource(BinaryValueWriter &out, const Data::SourceDecl &decl) {
  const auto key = Data::ContentKey(decl, Data::Address::Whole(0));
  return !key.empty() && out.Text(key) &&
         out(static_cast<int32_t>(decl.Wire),
             static_cast<int32_t>(decl.How),
             static_cast<int32_t>(decl.Order),
             static_cast<int32_t>(decl.OnAbsent),
             static_cast<int32_t>(decl.TileAbsence),
             static_cast<int32_t>(decl.MinZoom),
             static_cast<int32_t>(decl.MaxZoom),
             static_cast<int32_t>(decl.AncestorFill)) &&
         out.Text(decl.Schema);
}
}

std::string AssetSourceRecipe(std::string_view version,
                              const Data::SourceSet &sources,
                              std::span<const Data::DataKind> kinds) {
  uint64_t selected = 0;
  for (size_t at = 0; at < sources.Count(); ++at) {
    selected += std::ranges::find(kinds, sources.At(at).Declaration().Kind) != kinds.end();
  }
  BinaryValueWriter out(std::numeric_limits<size_t>::max());
  if (!out.Text("asset-source-recipe-2") || !out.Text(version) || !out(selected)) { return {}; }
  for (size_t at = 0; at < sources.Count(); ++at) {
    const auto &decl = sources.At(at).Declaration();
    if (std::ranges::find(kinds, decl.Kind) == kinds.end()) { continue; }
    if (!WriteSource(out, decl)) { return {}; }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}
