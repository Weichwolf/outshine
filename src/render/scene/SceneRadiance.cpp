#include "SceneRenderer.h"
#include "RadianceFilter.h"
#include <expected>
#include <memory>
#include <string>

namespace outshine::Render {
std::expected<std::shared_ptr<const RadianceImage>, std::string>
SceneRenderer::FilterSceneRadiance(Extent target) {
  const auto *source = LinearSource();
  if (!Device_ || !ActiveState().Ready || !Drew() || source == nullptr) {
    return std::unexpected("radiance filtering requires a submitted scene-linear frame");
  }
  if (!RadianceFilter_) { RadianceFilter_ = std::make_unique<RadianceFilter>(); }
  auto filtered = RadianceFilter_->Apply(*Device_, *source, target);
  if (!filtered) { return std::unexpected(filtered.error().Message); }
  return *filtered;
}
}
