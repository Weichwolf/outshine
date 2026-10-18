#include "WaterField.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
std::optional<bool> Flag(const OsmField &field, const OsmField::Feature &feature, const char *key) {
  const double number = field.Num(feature, key, std::numeric_limits<double>::quiet_NaN());
  if (number == 0 || number == 1) { return number == 1; }
  const auto text = field.Str(feature, key);
  if (text == "yes" || text == "true" || text == "1") { return true; }
  if (text == "no" || text == "false" || text == "0") { return false; }
  return std::nullopt;
}

WaterAsset::Input InputAt(const OsmField &field, size_t at) {
  const auto &feature = field.Features()[at];
  const auto &tile = field.Tiles()[feature.Tile];
  assert(at >= tile.FirstFeature && at - tile.FirstFeature < tile.FeatureCount);
  const auto crossing = field.Str(feature, "brunnel");
  return {.Source = feature.Tile,
          .Feature = static_cast<uint32_t>(at - tile.FirstFeature),
          .FeatureId = feature.ProviderFeatureId,
          .Layer = std::string(field.LayerName(static_cast<int>(feature.Layer))),
          .Kind = std::string(field.Str(feature, "kind")),
          .Crossing = std::string(crossing),
          .Bridge = crossing == "bridge" || Flag(field, feature, "bridge").value_or(false),
          .Tunnel = crossing == "tunnel" || Flag(field, feature, "tunnel").value_or(false),
          .Intermittent = Flag(field, feature, "intermittent")};
}
}

void WaterField::BindInputs(WaterAsset::Data &data, const OsmField &field) const {
  data.Sources.reserve(field.Tiles().size());
  for (const auto &tile : field.Tiles()) {
    data.Sources.push_back({.Identity = tile.Source,
                            .InputDigest = tile.InputDigest,
                            .FeatureCount = tile.FeatureCount});
  }
  std::vector<size_t> features;
  features.reserve(SurfaceFeatures_.size() + CourseFeatures_.size());
  features.insert(features.end(), SurfaceFeatures_.begin(), SurfaceFeatures_.end());
  features.insert(features.end(), CourseFeatures_.begin(), CourseFeatures_.end());
  std::ranges::sort(features);
  features.erase(std::ranges::unique(features).begin(), features.end());
  assert(features.size() <= WaterAsset::NoInput);
  data.Inputs.reserve(features.size());
  for (const size_t at : features) { data.Inputs.push_back(InputAt(field, at)); }
  const auto index = [&](size_t feature) {
    const auto found = std::ranges::lower_bound(features, feature);
    assert(found != features.end() && *found == feature);
    return static_cast<uint32_t>(found - features.begin());
  };
  assert(data.Surfaces.size() == SurfaceFeatures_.size());
  assert(data.Courses.size() == CourseFeatures_.size());
  for (size_t at = 0; at < data.Surfaces.size(); ++at) {
    data.Surfaces[at].Input = index(SurfaceFeatures_[at]);
  }
  for (size_t at = 0; at < data.Courses.size(); ++at) {
    data.Courses[at].Input = index(CourseFeatures_[at]);
  }
}
}
