#include "RoadHarvest.h"

#include <string_view>
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
    if ((int)feature.Layer != streets) { continue; }
    if (feature.Type != 2) {
      ++out.NotALine;
      continue;
    }
    const VegetationTemplates::Rule *const rule =
        widths.Find(field.LayerName((int)feature.Layer), field.Str(feature, "kind"));
    if (rule == nullptr || !(rule->WidthM > 0.0f)) {
      ++out.Unclassed;
      continue;
    }
    const double widthM = (double)rule->WidthM;
    if (widthM < bodyWidthM) {
      ++out.TooNarrow;
      if (widthM > out.WidestRefusedM) { out.WidestRefusedM = widthM; }
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
      if (layer < out.DeepestLayer) { out.DeepestLayer = layer; }
      if (layer > out.HighestLayer) { out.HighestLayer = layer; }
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
      along.reserve(ring.Count * 2);
      for (uint32_t point = 0; point < ring.Count; ++point) {
        along.push_back(field.Points()[2 * ((size_t)ring.First + point)]);
        along.push_back(field.Points()[2 * ((size_t)ring.First + point) + 1]);
      }
      Path::WayClass carries;
      carries.HalfWidthM = 0.5 * widthM;
      carries.MaxGradient = (double)rule->MaxGradient;
      carries.MinRadiusM = (double)rule->MinRadiusM;
      carries.Friction = (double)widths.FrictionOf((size_t)rule->Tpl);
      carries.Lanes = rule->Lanes;
      carries.Spans = bridge > 0.5 || tunnel > 0.5;
      into.Lay(along, carries);
      ++out.Ways;
      out.Points += ring.Count;
    }
  }
  return out;
}

}
