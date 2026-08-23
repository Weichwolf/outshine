#ifndef OUTSHINE_GENERATORS_FEATUREFIELD_H
#define OUTSHINE_GENERATORS_FEATUREFIELD_H

#include <cstdint>
#include <memory>
#include <vector>

#include "FeatureLevel.h"
#include "Span.h"

namespace outshine::Generators {

enum class FeatureKind : uint8_t { Structure, Water, Way };

enum class FeatureForm : uint8_t { Area, Ribbon };

class FeatureField {
public:
  struct Vertex {
    float Em, Nm;
  };
  struct Ring {
    uint32_t First, Count;
  };

  struct Feature {
    uint32_t FirstRing, RingCount;
    int32_t CoverRow;
    FeatureKind Kind;
    FeatureForm Form;
    float HalfWidthM;
    FeatureLevel Base;
    FeatureLevel Top;
    float MinEm, MinNm, MaxEm, MaxNm;
  };

  static std::shared_ptr<const FeatureField> Of(Span<const Feature> features,
                                                Span<const Ring> rings,
                                                Span<const Vertex> vertices);

  [[nodiscard]] size_t Count() const { return Features_.size(); }
  [[nodiscard]] const Feature &At(size_t i) const { return Features_[i]; }
  [[nodiscard]] Span<const Ring> Rings(const Feature &f) const;
  [[nodiscard]] Span<const Vertex> Vertices(const Ring &r) const;
  [[nodiscard]] static bool Boxed(const Feature &f, double eastM, double northM) noexcept {
    return eastM >= f.MinEm && eastM <= f.MaxEm && northM >= f.MinNm && northM <= f.MaxNm;
  }
  [[nodiscard]] bool Contains(const Feature &f, double eastM, double northM) const noexcept;

  [[nodiscard]] size_t HeapBytes() const;

private:
  FeatureField(Span<const Feature>, Span<const Ring>, Span<const Vertex>);

  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<Vertex> Vertices_;
};

}
#endif
