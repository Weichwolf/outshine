#ifndef FEATUREFIELD_H
#define FEATUREFIELD_H

#include <cstdint>
#include <memory>
#include <vector>

#include "FeatureLevel.h"
#include "Span.h"

namespace outshine::Generators {

/* WHAT A FEATURE IS, which is the only thing that separates the three producers' outlines from one
 * another. The cover row cannot: a house, the street beside it and the car park behind it all
 * classify as sealed ground. Nor can the presence of an upper surface: a house and a lake both have
 * one. What a thing IS comes from the vector layer it arrived in, and the class model is what maps a
 * layer to a meaning — which is why this is decided where the field is cut and never in a
 * generator. */
enum class FeatureKind : uint8_t { Structure, Water, Way };

/* HOW THE GEOMETRY READS. An area is bounded by its own ring; a ribbon is a centreline plus the
 * width its class declares, and it is not a closed ring — buffering it into one at ingest would
 * store a polygon the source does not have and let it disagree with the width the classifier used. */
enum class FeatureForm : uint8_t { Area, Ribbon };

class FeatureField {
public:
  struct Vertex {
    float Em, Nm;
  };
  struct Ring {
    uint32_t First, Count;
  };
  /* For an area the first ring is the outline and the rest are holes; for a ribbon each ring is one
   * way's centreline. The box is DERIVED from the rings at construction and never supplied: a box
   * and the outline it bounds cannot then disagree, and a generator looping features rejects almost
   * all of them without touching a vertex. */
  struct Feature {
    uint32_t FirstRing, RingCount;
    int32_t CoverRow;
    FeatureKind Kind;
    FeatureForm Form;
    float HalfWidthM;    /* Ribbon only, metres */
    FeatureLevel Base;   /* the ground this feature stands on; none where it follows the terrain */
    FeatureLevel Top;
    float MinEm, MinNm, MaxEm, MaxNm;
  };

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
