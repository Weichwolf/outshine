#include "TreePrototype.h"
#include "Check.h"
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const TreeLook defaults;
  const TreeSpecies species;
  const auto explicitLook = TreePrototype::LookOf(species);
  constexpr std::array<double, 3> barkSrgb{0.40, 0.31, 0.23};
  constexpr std::array<double, 3> leafLinear{
      0.05562605451408845, 0.09347127068377052, 0.039961625228706145};
  constexpr std::array<double, 3> leafTransmission{0.0634357550506689, 0.11687351146137416, 0.0};
  for (size_t channel = 0; channel < 3; ++channel) {
    const double linear = std::pow((barkSrgb[channel] + 0.055) / 1.055, 2.4);
    CHECK(std::abs(defaults.BarkRgb[channel] - linear) < 1e-7,
          "default bark follows the independent sRGB transfer function");
    CHECK(defaults.BarkRgb[channel] == explicitLook.BarkRgb[channel],
          "default and species-derived bark share the linear convention");
    CHECK(std::abs(defaults.LeafRgb[channel] - leafLinear[channel]) < 1e-7 &&
              defaults.LeafRgb[channel] == explicitLook.LeafRgb[channel],
          "owned Python optics remain linear without double conversion");
    CHECK_NEAR(defaults.LeafTransmission[channel],
               leafTransmission[channel],
               1e-7,
               "linear transmission",
               "owned default pigment optics match the independent Python sample");
    CHECK(defaults.LeafTransmission == explicitLook.LeafTransmission,
          "default and species-derived leaves retain their separate transmission");
  }
  CHECK(defaults.BarkRoughness == explicitLook.BarkRoughness &&
            defaults.LeafRoughness == explicitLook.LeafRoughness,
        "roughness defaults agree with species data");
  CHECK(defaults.BarkDark == explicitLook.BarkDark && defaults.BarkFreq == explicitLook.BarkFreq &&
            defaults.BarkRidge == explicitLook.BarkRidge &&
            defaults.LeafWidth == explicitLook.LeafWidth &&
            defaults.LeafWidest == explicitLook.LeafWidest &&
            defaults.LeafTip == explicitLook.LeafTip &&
            defaults.LeafBaseFill == explicitLook.LeafBaseFill &&
            defaults.LeafLobes == explicitLook.LeafLobes &&
            defaults.LeafLobeDepth == explicitLook.LeafLobeDepth &&
            defaults.LeafSerration == explicitLook.LeafSerration &&
            defaults.NeedleWidth == explicitLook.NeedleWidth,
        "shape parameters share species defaults");
  return Report();
}
