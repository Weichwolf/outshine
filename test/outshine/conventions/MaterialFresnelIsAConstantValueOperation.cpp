#include <scene/Material.h>
#include "Check.h"
#include <cmath>

namespace {
constexpr bool UnboundIndex() {
  const outshine::MaterialInstance empty;
  return !empty.bound() && empty.index() == -1 && outshine::MaterialInstance(3).bound();
}

static_assert(UnboundIndex());
constexpr outshine::Material disabled{.Ior = 0};
static_assert(outshine::DielectricF90(disabled) == 0);
static_assert(noexcept(outshine::DielectricF90(disabled)));

constexpr auto NormalReflectance() {
  outshine::Vec3f result;
  outshine::DielectricF0(outshine::Material{}, result);
  return result;
}

constexpr auto reflectance = NormalReflectance();
static_assert(reflectance[0] > 0.03999f && reflectance[0] < 0.04001f);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (float channel : reflectance) {
    CHECK(std::abs(channel - 0.04f) < 1e-7f, "IOR 1.5 gives four percent normal reflectance");
  }
  Vec3f result{{1, 2, 3}};
  DielectricF0(disabled, result);
  CHECK(result == Vec3f{}, "disabled dielectric Fresnel overwrites every channel");
  Material tinted;
  tinted.SpecularFactor = 0.5f;
  tinted.SpecularColour = {{1, 0.5f, 0.25f}};
  DielectricF0(tinted, result);
  CHECK(std::abs(result[0] - 0.02f) < 1e-7f && std::abs(result[1] - 0.01f) < 1e-7f &&
            std::abs(result[2] - 0.005f) < 1e-7f && DielectricF90(tinted) == 0.5f,
        "dielectric tint and strength preserve their independent channel factors");
  return Report();
}
