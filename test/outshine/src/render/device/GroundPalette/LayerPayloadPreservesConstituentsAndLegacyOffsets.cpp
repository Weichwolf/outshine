#include "GroundPalette.h"
#include "VegetationTemplates.h"
#include "Check.h"
#include <bit>
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
  const Vec4f fallback{{.11f, .22f, .33f, .9f}};
  const auto palette = Render::GroundMaterialPalette(templates, fallback);
  const size_t rows = templates.TemplateCount();
  const size_t slopesAt = 4u * (rows + 2u);
  const size_t surfacesAt = slopesAt + rows + 1u;
  const auto layersAt = std::bit_cast<uint32_t>(palette[3]);
  CHECK(std::bit_cast<uint32_t>(palette[0]) == rows, "native row count retains its bit contract");
  CHECK(layersAt == surfacesAt + 8u * (rows + 1u),
        "the reserved header points past the unchanged class/slope/surface prefix");
  CHECK(palette.size() == layersAt + 8u * (rows + 1u),
        "each material has one bounded 32-byte constituent payload");
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
                 1e-7,
                 "uncovered constituent",
                 "linear colour and perceptual roughness survive packing");
      CHECK_NEAR(storedMean + (1 - coverage) * difference,
                 source.Litter[channel],
                 1e-7,
                 "covering constituent",
                 "linear colour and perceptual roughness survive packing");
      CHECK(palette[surfacesAt + 8u * row + channel] == source.GroundSurf[channel] &&
                palette[layersAt + 8u * row + 4u + channel] == source.LitterSurf[channel],
            "both materials retain their grain size, bump amplitude and metric wavelengths");
    }
    CHECK(palette[slopesAt + row] == source.Edge[3] &&
              palette[surfacesAt + 8u * row + 5u] == source.Edge[0] &&
              palette[surfacesAt + 8u * row + 6u] == source.Edge[1],
          "slope exposure and constructed/natural boundary contracts keep their offsets");
  }
  for (size_t channel = 0; channel < 4; ++channel) {
    CHECK(palette[4u * (rows + 1u) + channel] == fallback[channel],
          "unknown appearance remains the explicit regional fallback");
    CHECK(palette[layersAt + 8u * rows + channel] == 0 &&
              palette[layersAt + 8u * rows + 4u + channel] == 0,
          "unknown landcover does not invent a litter material");
  }
  return Report();
}
