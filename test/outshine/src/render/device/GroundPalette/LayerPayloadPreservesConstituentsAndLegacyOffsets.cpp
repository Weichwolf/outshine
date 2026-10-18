#include "GroundPalette.h"
#include "VegetationTemplates.h"
#include "Check.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped material catalog");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "shipped ground recipes");
  if (!templates.Ready()) { return Report(); }
  constexpr double kConstituentTolerance = 1e-7;
  constexpr double kDensityTolerance = 2e-7;
  const Vec4f fallback{{.11f, .22f, .33f, .9f}};
  const auto palette = Render::GroundMaterialPalette(templates, fallback);
  const size_t rows = templates.TemplateCount();
  const size_t slopesAt = 4u * (rows + 2u);
  const size_t surfacesAt = slopesAt + rows + 1u;
  const auto layersAt = std::bit_cast<uint32_t>(palette[3]);
  CHECK(std::bit_cast<uint32_t>(palette[0]) == rows, "native row count retains its bit contract");
  CHECK(layersAt == surfacesAt + 8u * (rows + 1u),
        "the reserved header points past the unchanged class/slope/surface prefix");
  CHECK(palette.size() == layersAt + (8u + 24u) * (rows + 1u),
        "constituent prefix and bounded canopy tail retain their complete row counts");
  const size_t canopyAt = layersAt + 8u * (rows + 1u);
  size_t activeCanopies = 0;
  for (size_t row = 0; row < rows; ++row) {
    const auto &source = templates.Rows()[row];
    const auto mean = source.MeanGround();
    const float coverage = palette[surfacesAt + 8u * row + 7u];
    CHECK(coverage == source.Mix[0], "the shader receives the declared litter coverage");
    for (size_t channel = 0; channel < 4; ++channel) {
      const float storedMean = palette[4u * (row + 1u) + channel];
      const float difference = palette[layersAt + 8u * row + channel];
      CHECK(storedMean == mean[channel], "legacy material appearance remains the exact mean");
      CHECK_NEAR(storedMean - coverage * difference,
                 source.Ground[channel],
                 kConstituentTolerance,
                 "uncovered constituent",
                 "linear colour and perceptual roughness survive packing");
      CHECK_NEAR(storedMean + (1 - coverage) * difference,
                 source.Litter[channel],
                 kConstituentTolerance,
                 "covering constituent",
                 "linear colour and perceptual roughness survive packing");
      CHECK(palette[surfacesAt + 8u * row + channel] == source.GroundSurf[channel] &&
                palette[layersAt + 8u * row + 4u + channel] == source.LitterSurf[channel],
            "both materials retain their grain size, bump amplitude and metric wavelengths");
    }
    const auto coat = palette.begin() + static_cast<std::ptrdiff_t>(canopyAt + 24u * row);
    if (source.CanopyWeight == 0.0f) {
      CHECK(std::all_of(coat, coat + 24, [](float value) { return value == 0.0f; }),
            "noncanopy classes receive an entirely disabled tail record");
    } else {
      ++activeCanopies;
      for (size_t channel = 0; channel < 4; ++channel) {
        CHECK(coat[static_cast<std::ptrdiff_t>(channel)] == source.GroundBase[channel] &&
                  coat[static_cast<std::ptrdiff_t>(4u + channel)] == source.LitterBase[channel],
              "canopy gaps retain the original uncomposed substrate and litter");
        CHECK(coat[static_cast<std::ptrdiff_t>(8u + channel)] == source.Grass[channel] &&
                  coat[static_cast<std::ptrdiff_t>(12u + channel)] == source.Dry[channel],
              "green/dry reflectance and original density/height retain their offsets");
      }
      CHECK(coat[16] == source.Param[1] && coat[17] == source.Param[0] &&
                coat[18] == source.Param[3] && coat[19] == source.CanopyWeight,
            "shape and coarse mix retain the agreed float offsets");
      CHECK_NEAR(coat[20] * coat[20] * source.Grass[3],
                 1.0,
                 kDensityTolerance,
                 "density pitch",
                 "the microrecipe uses square metres consistently");
      CHECK(coat[21] >= 0.0f && coat[21] <= source.Dry[3] && coat[21] <= source.Param[1] &&
                coat[21] <= source.LitterSurf[1],
            "microrelief stays within the authored height, width and substrate amplitude");
      CHECK(coat[22] >= source.Dry[3] && coat[22] >= 2.0f * coat[20] &&
                coat[23] >= source.Param[1] && coat[23] >= coat[20],
            "metric filter wavelengths retain both stand scale and density support");
    }
    CHECK(palette[slopesAt + row] == source.Edge[3] &&
              palette[surfacesAt + 8u * row + 5u] == source.Edge[0] &&
              palette[surfacesAt + 8u * row + 6u] == source.Edge[1],
          "slope exposure and constructed/natural boundary contracts keep their offsets");
  }
  CHECK(activeCanopies == 2, "only meadow and declared alpine grassland gain a coat");
  CHECK(std::all_of(palette.begin() + static_cast<std::ptrdiff_t>(canopyAt + 24u * rows),
                    palette.end(),
                    [](float value) { return value == 0.0f; }),
        "fallback tail remains completely zero");
  for (size_t channel = 0; channel < 4; ++channel) {
    CHECK(palette[4u * (rows + 1u) + channel] == fallback[channel],
          "unknown appearance remains the explicit regional fallback");
    CHECK(palette[layersAt + 8u * rows + channel] == 0 &&
              palette[layersAt + 8u * rows + 4u + channel] == 0,
          "unknown landcover does not invent a litter material");
  }
  return Report();
}
