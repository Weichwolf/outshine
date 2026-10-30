#ifndef OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#define OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "RadianceImage.h"
#include "RadianceSupport.h"
#include <optional>
#include "Viewing.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace outshine::Render {
class SceneRenderer;
class ResidentMaterialField;
} // namespace outshine::Render

namespace outshine::Test {
struct BuildingRadianceField {
  ImpostorCards Cards;
  double PrepareMs = 0;
  double GpuMs = 0;
  double SetupGpuMs = 0;
  double ConfigureMs = 0;
  double SourcePrepareMs = 0;
  double SourceUploadMs = 0;
  double RelightEncodeMs = 0;
  double WaitMs = 0;
  double ReadbackMs = 0;
  double SupportFitMs = 0;
  Vec3 SupportDepth{};
  std::optional<Render::RadiancePlaneFit> SupportFit;
  std::shared_ptr<const Render::ResidentMaterialField> Source;
  std::vector<float> SourceLinear;
  std::vector<float> SourceDepth;
  Render::Viewpoint SourceCamera;
  Render::Viewpoint LightingCamera;
  std::shared_ptr<const Render::RadianceImage> Radiance;
};

[[nodiscard]] Render::Viewpoint BuildingCaptureCamera(const nlohmann::json &description);
[[nodiscard]] Render::Viewpoint BuildingQueryCamera(const nlohmann::json &query);
[[nodiscard]] std::expected<std::optional<Render::Viewpoint>, std::string>
BuildingLightingObserver(const nlohmann::json &query);

[[nodiscard]] std::expected<std::vector<float>, std::string>
ReadMaterialFieldImage(Render::Native::Device &device, const Render::Native::Texture &image);

[[nodiscard]] std::expected<void, std::string>
UpdateBuildingRadianceSupport(BuildingRadianceField &field,
                              const ImpostorAtlas &atlas,
                              size_t view,
                              const nlohmann::json &query);

[[nodiscard]] std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(Render::SceneRenderer &renderer,
                        const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original = nullptr,
                        const BuildingRadianceField *resident = nullptr);
} // namespace outshine::Test
#endif
