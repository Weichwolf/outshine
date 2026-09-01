#include "FeatureField.h"

namespace outshine::Generators {

std::shared_ptr<const FeatureField> FeatureField::Of(Span<const Feature> features,
                                                     Span<const Ring> rings,
                                                     Span<const Vertex> vertices) {
  for (const Feature &f : features) {
    if (static_cast<size_t>(f.FirstRing) + f.RingCount > rings.Size()) { return nullptr; }
    for (uint32_t i = 0; i < f.RingCount; i++) {
      const Ring &r = rings[f.FirstRing + i];
      if (static_cast<size_t>(r.First) + r.Count > vertices.Size()) { return nullptr; }
    }
  }
  return std::shared_ptr<const FeatureField>(new FeatureField(features, rings, vertices));
}

FeatureField::FeatureField(Span<const Feature> features,
                           Span<const Ring> rings,
                           Span<const Vertex> vertices)
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

Span<const FeatureField::Ring> FeatureField::Rings(const Feature &f) const {
  return Span<const Ring>(Rings_.data() + f.FirstRing, f.RingCount);
}

Span<const FeatureField::Vertex> FeatureField::Vertices(const Ring &r) const {
  return Span<const Vertex>(Vertices_.data() + r.First, r.Count);
}

namespace {

double SegmentGapM2(double em, double nm, double e0, double n0, double e1, double n1) {
  const double de = e1 - e0;
  const double dn = n1 - n0;
  const double len2 = de * de + dn * dn;
  double t = 0.0;
  if (len2 > 0.0) {
    t = ((em - e0) * de + (nm - n0) * dn) / len2;
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  }
  const double ge = em - (e0 + t * de);
  const double gn = nm - (n0 + t * dn);
  return ge * ge + gn * gn;
}

} // namespace

bool FeatureField::Contains(const Feature &f, double eastM, double northM) const noexcept {
  if (!Boxed(f, eastM, northM)) { return false; }
  if (f.Form == FeatureForm::Ribbon) {
    const double reach2 = static_cast<double>(f.HalfWidthM) * static_cast<double>(f.HalfWidthM);
    for (const Ring &r : Rings(f)) {
      const Span<const Vertex> v = Vertices(r);
      for (size_t i = 0; i + 1 < v.Size(); i++) {
        if (SegmentGapM2(eastM, northM, v[i].Em, v[i].Nm, v[i + 1].Em, v[i + 1].Nm) <= reach2) {
          return true;
        }
      }
    }
    return false;
  }
  int crossings = 0;
  for (const Ring &r : Rings(f)) {
    const Span<const Vertex> v = Vertices(r);
    if (v.Size() < 3) { continue; }
    for (size_t i = 0, j = v.Size() - 1; i < v.Size(); j = i++) {
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
