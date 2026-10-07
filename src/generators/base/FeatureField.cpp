#include <span>
#include <cmath>
#include "FeatureField.h"
#include <algorithm>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

namespace {

bool ValidFeature(const FeatureField::Feature &feature) {
  if (feature.Kind != FeatureKind::Structure && feature.Kind != FeatureKind::Water &&
      feature.Kind != FeatureKind::Way) {
    return false;
  }
  if (feature.Form != FeatureForm::Area && feature.Form != FeatureForm::Ribbon) { return false; }
  if (!std::isfinite(feature.HalfWidthM) || feature.HalfWidthM < 0) { return false; }
  const auto base = feature.Base.AslM();
  const auto top = feature.Top.AslM();
  return (!base || std::isfinite(*base)) && (!top || std::isfinite(*top));
}

}

std::shared_ptr<const FeatureField> FeatureField::Of(std::span<const Feature> features,
                                                     std::span<const Ring> rings,
                                                     std::span<const Vertex> vertices) {
  for (const Vertex &vertex : vertices) {
    if (!std::isfinite(vertex.Em) || !std::isfinite(vertex.Nm)) { return nullptr; }
  }
  for (const Ring &ring : rings) {
    if (ring.First > vertices.size() || ring.Count > vertices.size() - ring.First) {
      return nullptr;
    }
  }
  for (const Feature &feature : features) {
    if (!ValidFeature(feature) || feature.FirstRing > rings.size() ||
        feature.RingCount > rings.size() - feature.FirstRing) {
      return nullptr;
    }
  }
  auto result = std::shared_ptr<FeatureField>(new FeatureField(features, rings, vertices));
  for (Feature &feature : result->Features_) {
    if (!result->SetBounds(feature)) { return nullptr; }
  }
  return result;
}

FeatureField::FeatureField(std::span<const Feature> features,
                           std::span<const Ring> rings,
                           std::span<const Vertex> vertices)
    : Features_(features.begin(), features.end()),
      Rings_(rings.begin(), rings.end()),
      Vertices_(vertices.begin(), vertices.end()) {}

bool FeatureField::SetBounds(Feature &f) const {
  f.MinEm = f.MinNm = 0.0f;
  f.MaxEm = f.MaxNm = -1.0f;
  bool first = true;
  for (const Ring &r : Rings(f)) {
    for (const Vertex &v : Vertices(r)) {
      if (first) {
        f.MinEm = f.MaxEm = v.Em;
        f.MinNm = f.MaxNm = v.Nm;
        first = false;
        continue;
      }
      f.MinEm = v.Em < f.MinEm ? v.Em : f.MinEm;
      f.MaxEm = v.Em > f.MaxEm ? v.Em : f.MaxEm;
      f.MinNm = v.Nm < f.MinNm ? v.Nm : f.MinNm;
      f.MaxNm = v.Nm > f.MaxNm ? v.Nm : f.MaxNm;
    }
  }

  if (first || f.Form != FeatureForm::Ribbon) { return true; }
  f.MinEm -= f.HalfWidthM;
  f.MaxEm += f.HalfWidthM;
  f.MinNm -= f.HalfWidthM;
  f.MaxNm += f.HalfWidthM;
  return std::isfinite(f.MinEm) && std::isfinite(f.MaxEm) && std::isfinite(f.MinNm) &&
         std::isfinite(f.MaxNm);
}

std::span<const FeatureField::Ring> FeatureField::Rings(const Feature &f) const {
  return std::span(Rings_).subspan(f.FirstRing, f.RingCount);
}

std::span<const FeatureField::Vertex> FeatureField::Vertices(const Ring &r) const {
  return std::span(Vertices_).subspan(r.First, r.Count);
}

namespace {

double SegmentGapM2(EastNorth at, EastNorth from, EastNorth to) {
  const double de = to.EastM - from.EastM;
  const double dn = to.NorthM - from.NorthM;
  const double len2 = de * de + dn * dn;
  double t = 0.0;
  if (len2 > 0.0) {
    t = ((at.EastM - from.EastM) * de + (at.NorthM - from.NorthM) * dn) / len2;
    t = std::clamp(t, 0.0, 1.0);
  }
  const double ge = at.EastM - (from.EastM + t * de);
  const double gn = at.NorthM - (from.NorthM + t * dn);
  return ge * ge + gn * gn;
}

}

bool FeatureField::Intersects(const Feature &f, EastNorth at, double radiusM) const noexcept {
  if (radiusM < 0.0 || !std::isfinite(radiusM)) { return false; }
  if (at.EastM + radiusM < f.MinEm || at.EastM - radiusM > f.MaxEm ||
      at.NorthM + radiusM < f.MinNm || at.NorthM - radiusM > f.MaxNm) {
    return false;
  }
  if (f.Form == FeatureForm::Area && Contains(f, at)) { return true; }
  const double reachM = radiusM + (f.Form == FeatureForm::Ribbon ? f.HalfWidthM : 0.0);
  const double reachM2 = reachM * reachM;
  for (const Ring &ring : Rings(f)) {
    const auto vertices = Vertices(ring);
    if (vertices.size() < 2) { continue; }
    const size_t first = f.Form == FeatureForm::Area ? 0 : 1;
    for (size_t atVertex = first; atVertex < vertices.size(); ++atVertex) {
      const auto &from = vertices[atVertex];
      const auto &to = vertices[atVertex == 0 ? vertices.size() - 1 : atVertex - 1];
      if (SegmentGapM2(at,
                       {.EastM = from.Em, .NorthM = from.Nm},
                       {.EastM = to.Em, .NorthM = to.Nm}) <= reachM2) {
        return true;
      }
    }
  }
  return false;
}

bool FeatureField::Contains(const Feature &f, EastNorth at) const noexcept {
  const double eastM = at.EastM;
  const double northM = at.NorthM;
  if (!Boxed(f, at)) { return false; }
  if (f.Form == FeatureForm::Ribbon) { return Intersects(f, at, 0.0); }
  int crossings = 0;
  for (const Ring &r : Rings(f)) {
    const std::span<const Vertex> v = Vertices(r);
    if (v.size() < 3) { continue; }
    for (size_t i = 0, j = v.size() - 1; i < v.size(); j = i++) {
      const double ei = v[i].Em;
      const double ni = v[i].Nm;
      const double ej = v[j].Em;
      const double nj = v[j].Nm;
      if ((ni > northM) == (nj > northM)) { continue; }
      if (eastM < (ej - ei) * (northM - ni) / (nj - ni) + ei) { crossings++; }
    }
  }
  return (static_cast<uint32_t>(crossings) & 1) != 0;
}

size_t FeatureField::HeapBytes() const {
  return Features_.capacity() * sizeof(Feature) + Rings_.capacity() * sizeof(Ring) +
         Vertices_.capacity() * sizeof(Vertex);
}

}
