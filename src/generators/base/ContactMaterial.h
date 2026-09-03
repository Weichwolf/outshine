#ifndef OUTSHINE_GENERATORS_BASE_CONTACTMATERIAL_H
#define OUTSHINE_GENERATORS_BASE_CONTACTMATERIAL_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine::Generators {

constexpr size_t kBodyBytes = 48;

enum class ContactMaterial : uint32_t {};

struct Solid {
  double Em = 0.0, Nm = 0.0;
  double BaseAslM = 0.0;
  float RadiusM = 0.0f;
  float HeightM = 0.0f;
  float MassKg = 0.0f;
  float YawRad = 0.0f;
  ContactMaterial Contact = ContactMaterial{0};
};

static_assert(sizeof(Solid) == 3 * sizeof(double) + 4 * sizeof(float) + 2 * sizeof(uint32_t),
              "three doubles, four floats and a contact material, and the four bytes after it are "
              "the tail padding an eight-byte alignment costs");
static_assert(sizeof(Solid) == kBodyBytes, "sizeof(Solid)");
static_assert(std::is_trivially_copyable_v<Solid>, "collect is a memcpy");

} // namespace outshine::Generators
#endif
