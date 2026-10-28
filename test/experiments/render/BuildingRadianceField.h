#ifndef OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#define OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "RadianceImage.h"
#include <nlohmann/json_fwd.hpp>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace outshine::Render {
class SceneRenderer;
}

namespace outshine::Test {
struct BuildingRadianceField {
  ImpostorCards Cards;
  double PrepareMs = 0;
  double GpuMs = 0;
  double SetupGpuMs = 0;
  std::vector<float> SourceLinear;
  std::shared_ptr<const Render::RadianceImage> Radiance;
};

[[nodiscard]] std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(Render::SceneRenderer &renderer,
                        const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original = nullptr);
}
#endif
