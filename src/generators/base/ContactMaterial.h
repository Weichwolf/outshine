#ifndef OUTSHINE_GENERATORS_BASE_CONTACTMATERIAL_H
#define OUTSHINE_GENERATORS_BASE_CONTACTMATERIAL_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include "BodyId.h"

namespace outshine::Generators {

constexpr size_t kBodyBytes = 48;

enum class ContactMaterial : uint32_t {};

class Body {
public:
  double Em = 0.0, Nm = 0.0;
  double BaseAslM = 0.0;
  float RadiusM = 0.0f;
  float HeightM = 0.0f;
  float MassKg = 0.0f;
  float YawRad = 0.0f;
  ContactMaterial Contact = ContactMaterial{0};

  [[nodiscard]] std::optional<BodyId> Id() const {
    if (Id_ == kUnplaced) { return std::nullopt; }
    return BodyId(Id_);
  }

private:
  friend class OccupancySink;
  static constexpr uint32_t kUnplaced = 0xffffffffu;

  uint32_t Id_ = kUnplaced;
};

static_assert(sizeof(Body) == 3 * sizeof(double) + 4 * sizeof(float) + 2 * sizeof(uint32_t),
              "Body carries padding");
static_assert(sizeof(Body) == kBodyBytes, "sizeof(Body)");
static_assert(std::is_trivially_copyable<Body>::value, "collect is a memcpy");

} // namespace outshine::Generators
#endif
