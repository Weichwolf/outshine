#include <atomic>
#include <span>
#include "SceneRenderer.h"
#include "MaterialFieldPass.h"
#include <expected>
#include <memory>

namespace outshine::Render {
Native::Result<void> SceneRenderer::ConfigureMaterialFields() {
  if (!Stands()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Driver, .Message = ActiveState().WhyNot});
  }
  if (!MaterialFields_) {
    MaterialFields_ = std::make_unique<MaterialFieldPass>(*Device_, Uploads_.get());
  }
  return MaterialFields_->Configure();
}

Native::Result<std::shared_ptr<const ResidentMaterialField>>
SceneRenderer::PrepareMaterialField(const std::shared_ptr<const MaterialFieldInput> &input,
                                    const std::atomic_bool *stopping) {
  if (!MaterialFields_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field pipelines must be prepared first"});
  }
  return MaterialFields_->Prepare(input, stopping);
}

Native::Result<MaterialFieldImages>
SceneRenderer::RelightMaterialField(const ResidentMaterialField &source,
                                    const MaterialFieldView &view,
                                    const std::atomic_bool *stopping) {
  return RelightMaterialField(source, {}, view, stopping);
}

Native::Result<MaterialFieldImages> SceneRenderer::RelightMaterialField(
    const ResidentMaterialField &source,
    std::span<const std::shared_ptr<const ResidentMaterialField>> additional,
    const MaterialFieldView &view,
    const std::atomic_bool *stopping) {
  if (!MaterialFields_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field pipelines must be prepared first"});
  }
  return MaterialFields_->Relight(source, additional, view, stopping);
}

}
