#include "FeatureField.h"

namespace outshine::Generators {

std::shared_ptr<const FeatureField> FeatureField::Of(Span<const Feature> features,
                                                     Span<const Ring> rings,
                                                     Span<const Vertex> vertices) {
  for (const Feature &f : features) {
    if ((size_t)f.FirstRing + f.RingCount > rings.Size()) return nullptr;
    for (uint32_t i = 0; i < f.RingCount; i++) {
      const Ring &r = rings[f.FirstRing + i];
      if ((size_t)r.First + r.Count > vertices.Size()) return nullptr;
    }
  }
  return std::shared_ptr<const FeatureField>(new FeatureField(features, rings, vertices));
}

FeatureField::FeatureField(Span<const Feature> features, Span<const Ring> rings,
                           Span<const Vertex> vertices)
    : Features_(features.begin(), features.end()), Rings_(rings.begin(), rings.end()),
      Vertices_(vertices.begin(), vertices.end()) {
  for (Feature &f : Features_) {
    f.MinEm = f.MinNm = 0.0f;
    f.MaxEm = f.MaxNm = -1.0f;   /* empty, so a feature with no vertices bounds nothing */
    bool first = true;
    for (const Ring &r : Rings(f))
      for (const Vertex &v : Vertices(r)) {
        if (first) { f.MinEm = f.MaxEm = v.Em; f.MinNm = f.MaxNm = v.Nm; first = false; continue; }
        f.MinEm = v.Em < f.MinEm ? v.Em : f.MinEm;
        f.MaxEm = v.Em > f.MaxEm ? v.Em : f.MaxEm;
        f.MinNm = v.Nm < f.MinNm ? v.Nm : f.MinNm;
        f.MaxNm = v.Nm > f.MaxNm ? v.Nm : f.MaxNm;
      }
  }
}

Span<const FeatureField::Ring> FeatureField::Rings(const Feature &f) const {
  return Span<const Ring>(Rings_.data() + f.FirstRing, f.RingCount);
}

Span<const FeatureField::Vertex> FeatureField::Vertices(const Ring &r) const {
  return Span<const Vertex>(Vertices_.data() + r.First, r.Count);
}

bool FeatureField::Contains(const Feature &f, double eastM, double northM) const noexcept {
  if (!Boxed(f, eastM, northM)) return false;
  int crossings = 0;
  for (const Ring &r : Rings(f)) {
    const Span<const Vertex> v = Vertices(r);
    if (v.Size() < 3) continue;
    for (size_t i = 0, j = v.Size() - 1; i < v.Size(); j = i++) {
      const double ei = v[i].Em, ni = v[i].Nm, ej = v[j].Em, nj = v[j].Nm;
      if ((ni > northM) == (nj > northM)) continue;
      if (eastM < (ej - ei) * (northM - ni) / (nj - ni) + ei) crossings++;
    }
  }
  return (crossings & 1) != 0;
}

size_t FeatureField::HeapBytes() const {
  return Features_.capacity() * sizeof(Feature) + Rings_.capacity() * sizeof(Ring) +
         Vertices_.capacity() * sizeof(Vertex);
}

} // namespace outshine::Generators
