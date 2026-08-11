#ifndef FEATUREFIELD_H
#define FEATUREFIELD_H

#include <cstdint>
#include <memory>
#include <vector>

#include "FeatureTop.h"
#include "Span.h"

namespace outshine::Generators {

class FeatureField {
public:
  struct Vertex {
    float Em, Nm;
  };
  struct Ring {
    uint32_t First, Count;
  };
  /* The first ring is the outline; the rest are holes. The box is DERIVED from the rings at
   * construction and never supplied: a box and the outline it bounds cannot then disagree, and a
   * generator looping features rejects almost all of them without touching a vertex. */
  struct Feature {
    uint32_t FirstRing, RingCount;
    int32_t CoverRow;
    FeatureTop Top;
    float MinEm, MinNm, MaxEm, MaxNm;
  };

  /* Which features a generator reads: its own row of the declared table, and whether the thing has
   * an upper surface at all. Row alone does not separate a house from the street beside it — both
   * classify as sealed ground — and a top alone does not separate a house from a lake. */
  struct Sieve {
    int32_t CoverRow = -1;
    bool Topped = true;
  };
  bool Passes(const Feature &f, const Sieve &sieve) const noexcept;

  static std::shared_ptr<const FeatureField> Of(Span<const Feature> features,
                                                Span<const Ring> rings,
                                                Span<const Vertex> vertices);

  size_t Count() const { return Features_.size(); }
  const Feature &At(size_t i) const { return Features_[i]; }
  Span<const Ring> Rings(const Feature &f) const;
  Span<const Vertex> Vertices(const Ring &r) const;
  static bool Boxed(const Feature &f, double eastM, double northM) noexcept {
    return eastM >= f.MinEm && eastM <= f.MaxEm && northM >= f.MinNm && northM <= f.MaxNm;
  }
  bool Contains(const Feature &f, double eastM, double northM) const noexcept;

  size_t HeapBytes() const;

private:
  FeatureField(Span<const Feature>, Span<const Ring>, Span<const Vertex>);

  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<Vertex> Vertices_;
};

} // namespace outshine::Generators
#endif
