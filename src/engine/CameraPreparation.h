#ifndef OUTSHINE_ENGINE_CAMERAPREPARATION_H
#define OUTSHINE_ENGINE_CAMERAPREPARATION_H

#include "Earth.h"
#include "GroundSample.h"
#include "Viewing.h"
#include "Lens.h"
#include <expected>
#include <optional>
#include <string_view>

namespace outshine {
class GroundQuery;

namespace Scenario {
struct View;
}

inline constexpr std::string_view kInvalidCameraProjection =
    "the selected view declares an invalid or unrepresentable projection";
inline constexpr std::string_view kInvalidCameraTransform =
    "the declared camera has no valid local transform";

struct CameraPreparationContext {
  LongitudeLatitude Origin;
  double WidthPx = 0;
  double HeightPx = 0;
  const GroundQuery *Ground = nullptr;
  std::optional<GroundSample> HeightSample;
};

[[nodiscard]] std::expected<Render::Viewpoint, Render::LensError>
ProjectCamera(const Camera &camera, Render::Viewpoint view, double widthPx, double heightPx);

[[nodiscard]] std::expected<std::optional<Render::Viewpoint>, std::string_view>
PrepareCamera(const Scenario::View &seen, const CameraPreparationContext &context);
}
#endif
