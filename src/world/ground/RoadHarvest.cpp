#include "RoadHarvest.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Ground {

Reaped Reap(const OsmField &field,
            const VegetationTemplates &widths,
            double bodyWidthM,
            Path::Network &into) {
  Reaped out;
  const int streets = field.Layer(OsmLayer::Streets);
  if (streets < 0) {
    out.StreetsAbsent = true;
    return out;
  }

  std::vector<double> along;
  for (const OsmField::Feature &feature : field.Features()) {
    if (std::cmp_not_equal(feature.Layer, streets)) { continue; }
    if (feature.Type != 2) {
      ++out.NotALine;
      continue;
    }
    const VegetationTemplates::Rule *const rule =
        widths.Find(field.LayerName(static_cast<int>(feature.Layer)), field.Str(feature, "kind"));
    if (rule == nullptr || !(rule->WidthM > 0.0f)) {
      ++out.Unclassed;
      continue;
    }
    const auto widthM = static_cast<double>(rule->WidthM);
    if (widthM < bodyWidthM) {
      ++out.TooNarrow;
      out.WidestRefusedM = std::max(widthM, out.WidestRefusedM);
      continue;
    }
    const std::string_view kind = field.Str(feature, "kind");
    if (rule->Lanes <= 0) {
      ++out.NotACarriageway;
      if (!Listed(out.NotCarriageways, kind)) { out.NotCarriageways += std::string(kind) + " "; }
      continue;
    }
    if (!(rule->MaxGradient > 0.0)) {
      ++out.Ungraded;
      if (!Listed(out.WithoutGrade, kind)) { out.WithoutGrade += std::string(kind) + " "; }
    }
    const double bridge = field.Num(feature, "bridge", 0.0);
    const double tunnel = field.Num(feature, "tunnel", 0.0);
    const double layer = field.Num(feature, "layer", 0.0);
    out.Bridges += bridge > 0.5 ? 1u : 0u;
    out.Tunnels += tunnel > 0.5 ? 1u : 0u;
    if (layer != 0.0) {
      ++out.Layered;
      out.DeepestLayer = std::min(layer, out.DeepestLayer);
      out.HighestLayer = std::max(layer, out.HighestLayer);
    }
    if (!(rule->MinRadiusM > 0.0f)) {
      ++out.Undesigned;
      if (!Listed(out.WithoutRadius, kind)) { out.WithoutRadius += std::string(kind) + " "; }
    }
    if (!(out.NarrowestTakenM > 0.0) || widthM < out.NarrowestTakenM) {
      out.NarrowestTakenM = widthM;
    }

    for (uint32_t which = 0; which < feature.RingCount; ++which) {
      const OsmField::Ring &ring = field.Rings()[feature.FirstRing + which];
      if (ring.Count < 2) { continue; }
      along.clear();
      along.reserve(static_cast<size_t>(ring.Count) * 2);
      for (uint32_t point = 0; point < ring.Count; ++point) {
        along.push_back(field.Points()[2 * (static_cast<size_t>(ring.First) + point)]);
        along.push_back(field.Points()[2 * (static_cast<size_t>(ring.First) + point) + 1]);
      }
      Path::WayClass carries;
      carries.HalfWidthM = 0.5 * widthM;
      carries.MaxGradient = static_cast<double>(rule->MaxGradient);
      carries.MinRadiusM = static_cast<double>(rule->MinRadiusM);
      carries.Friction = static_cast<double>(widths.FrictionOf(static_cast<size_t>(rule->Tpl)));
      carries.Lanes = rule->Lanes;
      carries.Spans = bridge > 0.5 || tunnel > 0.5;
      into.Lay(along, carries);
      ++out.Ways;
      out.Points += ring.Count;
    }
  }
  return out;
}

} // namespace outshine::Ground
