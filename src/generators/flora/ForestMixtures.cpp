#include "ForestMixtures.h"
#include "TreeSpecies.h"
#include "VegetationTemplates.h"
#include "ReadTextFile.h"
#include "Json.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {

namespace {
std::expected<SpeciesMixture, std::string>
ReadMixture(Json::Ref weights, std::span<const std::string_view> names, std::string_view name) {
  if (weights.GetKind() != Json::Kind::Object) {
    return std::unexpected(
        std::format("forest template requires explicit species weights: {}", name));
  }
  std::vector<double> bound(names.size(), 0);
  std::vector<bool> assigned(names.size(), false);
  for (size_t entry = 0; entry < weights.Size(); ++entry) {
    const auto key = weights.Key(entry);
    const auto found = std::ranges::find(names, key);
    if (found == names.end()) {
      return std::unexpected(
          std::format("forest template {} names unknown species: {}", name, key));
    }
    const auto index = static_cast<size_t>(found - names.begin());
    if (assigned[index]) {
      return std::unexpected(std::format("forest template {} repeats species: {}", name, key));
    }
    const auto value = weights[key.c_str()];
    const auto number = value.Num();
    if (value.GetKind() != Json::Kind::Number || !std::isfinite(number) || number < 0) {
      return std::unexpected(
          std::format("forest template {} has invalid weight for: {}", name, key));
    }
    bound[index] = number;
    assigned[index] = true;
  }
  return SpeciesMixture::Prepare(names, bound);
}
}

std::expected<std::vector<SpeciesMixture>, std::string>
ReadForestMixtures(std::string_view path,
                   const ::outshine::Ground::VegetationTemplates &templates,
                   std::span<const TreeSpecies> species) {
  if (species.empty() || species.size() > SpeciesMixture::kMostSpecies) {
    return std::unexpected("forest mixtures require 1..64 catalogue species");
  }
  constexpr size_t kDefinitionsMostBytes = size_t{1024} * 1024;
  const auto text = ReadTextFile(path, kDefinitionsMostBytes);
  if (!text) { return std::unexpected(text.error()); }
  Json document;
  if (!document.Parse(text->data(), text->size())) {
    return std::unexpected("forest mixtures require a valid vegetation definition");
  }
  const auto definitions = document.Root()["templates"];
  if (definitions.GetKind() != Json::Kind::Array ||
      definitions.Size() > templates.TemplateCount()) {
    return std::unexpected("forest mixture definitions do not match the ground templates");
  }
  std::vector<std::string_view> names;
  names.reserve(species.size());
  for (const auto &entry : species) { names.push_back(entry.Name()); }
  std::vector<SpeciesMixture> result(templates.TemplateCount());
  std::vector<bool> seen(templates.TemplateCount(), false);
  for (size_t at = 0; at < definitions.Size(); ++at) {
    const auto source = definitions[at];
    const auto name = source["name"].Str();
    size_t row = 0;
    while (row < templates.TemplateCount() && templates.Name(row) != name) { ++row; }
    if (row == templates.TemplateCount() || seen[row]) {
      return std::unexpected("unknown or repeated forest template: " + name);
    }
    seen[row] = true;
    auto mixture = ReadMixture(source["trees"]["species"], names, name);
    if (!mixture) { return std::unexpected(mixture.error()); }
    result[row] = std::move(*mixture);
  }
  for (size_t row = 0; row < seen.size(); ++row) {
    if (!seen[row] && static_cast<int>(row) != templates.UnmappedRow()) {
      return std::unexpected("forest mixture is missing template: " + templates.Name(row));
    }
  }
  return result;
}

}
