#ifndef OUTSHINE_TEST_PLANTRECIPEJSONFIXTURE_H
#define OUTSHINE_TEST_PLANTRECIPEJSONFIXTURE_H
#include <string_view>

namespace outshine::Test {
inline constexpr std::string_view kPlantRecipeJson = R"recipe({
  "version": "branching-plant-2",
  "seed": 17,
  "levels": [
    {
      "branches": 1,
      "length": 1.55,
      "base_radius": 0.019,
      "tip_radius": 0.006,
      "tip_scale": 1,
      "length_shape": "taper",
      "attachment_jitter": 0.0,
      "azimuth_variation": 0.18,
      "upward_bias": 0.0,
      "first": 0.2,
      "last": 0.92,
      "angle": 1.1,
      "angle_variation": 0.18,
      "bend": 0.08,
      "bend_variation": 0.035,
      "leaf_internode": 0.2,
      "leaf_first": 0.1,
      "leaf_last": 0.96,
      "leaf_angle": 1.12,
      "leaf_sky_alignment": 0.65,
      "arrangement": "spiral"
    }
  ],
  "blades": [
    {
      "length": 0.102,
      "width": 0.0528,
      "widest": 0.62,
      "lobes": 6,
      "lobe_depth": 0.35,
      "base_fill": 0.03,
      "bend": 0.18,
      "fold": 0.045,
      "fold_power": 2,
      "twist": -0.28,
      "petiole": 0.004
    }
  ]
})recipe";
}
#endif
