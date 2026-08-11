#ifndef BODY_H
#define BODY_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include "BodyId.h"

namespace outshine::Generators {

/* A row of the declared contact table, not a taxonomy: what a contact does is physics, and which
 * row a body carries is a declaration. */
enum class ContactMaterial : uint32_t {};

class Body {
public:
  double Em = 0.0, Nm = 0.0;    /* metres east and north of the region anchor */
  double BaseAslM = 0.0;        /* the DEM's own datum, metres */
  float RadiusM = 0.0f;         /* the substitute contact cylinder */
  float HeightM = 0.0f;
  float MassKg = 0.0f;
  float YawRad = 0.0f;
  ContactMaterial Contact = ContactMaterial{0};

  /* Empty until a sink has claimed the space: a proposal and a placement are the same type, and
   * only one of them may be drawn. */
  [[nodiscard]] std::optional<BodyId> Id() const {
    if (Id_ == kUnplaced) return std::nullopt;
    return BodyId(Id_);
  }

private:
  friend class OccupancySink;
  static constexpr uint32_t kUnplaced = 0xffffffffu;

  uint32_t Id_ = kUnplaced;
};

/* Placed() is compared and copied as bytes, which a hole in the layout would make a lie. */
static_assert(sizeof(Body) == 3 * sizeof(double) + 4 * sizeof(float) + 2 * sizeof(uint32_t),
              "Body carries padding");
static_assert(sizeof(Body) == 48, "sizeof(Body)");
static_assert(std::is_trivially_copyable<Body>::value, "collect is a memcpy");

} // namespace outshine::Generators
#endif
