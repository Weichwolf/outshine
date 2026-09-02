#include <span>
#include "FeatureField.h"
#include <algorithm>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

std::shared_ptr<const FeatureField> FeatureField::Of(std::span<const Feature> features,
                                                     std::span<const Ring> rings,
                                                     std::span<const Vertex> vertices) {
  for (const Feature &f : features) {
    if (static_cast<size_t>(f.FirstRing) + f.RingCount > rings.size()) { return nullptr; }
    for (uint32_t i = 0; i < f.RingCount; i++) {
      const Ring &r = rings[f.FirstRing + i];
      if (static_cast<size_t>(r.First) + r.Count > vertices.size()) { return nullptr; }
    }
  }
  return std::shared_ptr<const FeatureField>(new FeatureField(features, rings, vertices));
}

FeatureField::FeatureField(std::span<const Feature> features,
                           std::span<const Ring> rings,
                           std::span<const Vertex> vertices)
    : Features_(features.begin(), features.end()),
      Rings_(rings.begin(), rings.end()),
      Vertices_(vertices.begin(), vertices.end()) {
  for (Feature &f : Features_) {
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

    if (first || f.Form != FeatureForm::Ribbon) { continue; }
    f.MinEm -= f.HalfWidthM;
    f.MaxEm += f.HalfWidthM;
    f.MinNm -= f.HalfWidthM;
    f.MaxNm += f.HalfWidthM;
  }
}

std::span<const FeatureField::Ring> FeatureField::Rings(const Feature &f) const {
  return {Rings_.data() + f.FirstRing, f.RingCount};
}

std::span<const FeatureField::Vertex> FeatureField::Vertices(const Ring &r) const {
  return {Vertices_.data() + r.First, r.Count};
}

namespace {

double SegmentGapM2(double em, double nm, double e0, double n0, double e1, double n1) {
  const double de = e1 - e0;
  const double dn = n1 - n0;
  const double len2 = de * de + dn * dn;
  double t = 0.0;
  if (len2 > 0.0) {
    t = ((em - e0) * de + (nm - n0) * dn) / len2;
    t = std::clamp(t, 0.0, 1.0);
  }
  const double ge = em - (e0 + t * de);
  const double gn = nm - (n0 + t * dn);
  return ge * ge + gn * gn;
}

} // namespace

bool FeatureField::Contains(const Feature &f, EastNorth at) const noexcept {
  const double eastM = at.EastM;
  const double northM = at.NorthM;
  if (!Boxed(f, at)) { return false; }
  if (f.Form == FeatureForm::Ribbon) {
    const double reach2 = static_cast<double>(f.HalfWidthM) * static_cast<double>(f.HalfWidthM);
    for (const Ring &r : Rings(f)) {
      const std::span<const Vertex> v = Vertices(r);
      for (size_t i = 0; i + 1 < v.size(); i++) {
        if (SegmentGapM2(eastM, northM, v[i].Em, v[i].Nm, v[i + 1].Em, v[i + 1].Nm) <= reach2) {
          return true;
        }
      }
    }
    return false;
  }
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

} // namespace outshine::Generators
