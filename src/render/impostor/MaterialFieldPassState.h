#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDPASSSTATE_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDPASSSTATE_H
#include "MaterialFieldPass.h"
#include "GeometryRenderer.h"
#include "MaterialFieldRaster.h"
#include "RadianceFilter.h"
#include "RadianceIntegral.h"
#include "RenderScene.h"
#include <thread>
#include <vector>

namespace outshine::Render {
inline constexpr double kMaterialFieldUnitTolerance = 1e-6;
[[nodiscard]] bool ValidMaterialFieldLighting(const MaterialFieldView &view) noexcept;
[[nodiscard]] bool SameMaterialFieldCapture(const Viewpoint &capture,
                                            const Viewpoint &view) noexcept;

struct MaterialFieldPipelines {
  explicit MaterialFieldPipelines(Native::Device &device) noexcept : Device(device) {}

  Native::Device &Device;
  GeometryPipelineBinding Geometry;
  std::unique_ptr<Native::GraphicsPipeline> Raster;
  RadianceFilter Filter;
  RadianceIntegral Integral;
  std::shared_ptr<Native::Sampler> Sampling;
  bool Ready = false;
};

struct ResidentMaterialField::Data {
  explicit Data(std::shared_ptr<MaterialFieldPipelines> pipelines)
      : Pipelines(std::move(pipelines)), Renderer(Scene) {}

  std::shared_ptr<MaterialFieldPipelines> Pipelines;
  RenderScene Scene;
  GeometryRenderer Renderer;
  Extent Raster;
  std::unique_ptr<MaterialFieldRaster> Stored;
  std::optional<Viewpoint> Capture;
  Native::Completion Produced;

  [[nodiscard]] Native::Result<void>
  Encode(const MaterialFieldView &view, const FrameContext &frame, const PassRecording &recording);
};

struct MaterialFieldPass::State {
  State(Native::Device &device, UploadQueue *uploads)
      : Pipelines(std::make_shared<MaterialFieldPipelines>(device)),
        Uploads(uploads),
        Owner(std::this_thread::get_id()) {}

  struct Pending {
    Native::Completion Completion;
    std::shared_ptr<ResidentMaterialField::Data> Source;
    MaterialFieldImages Images;
    std::shared_ptr<Native::Texture> Scratch = nullptr;
  };

  std::shared_ptr<MaterialFieldPipelines> Pipelines;
  UploadQueue *Uploads;
  std::thread::id Owner;
  std::vector<Pending> InFlight;
  std::optional<Native::Completion> Last;

  [[nodiscard]] Native::Result<void> PrepareGeometry(ResidentMaterialField::Data &source,
                                                     const MaterialFieldInput &input,
                                                     Native::Commands &commands) const;

  [[nodiscard]] bool OnOwner() const noexcept { return Owner == std::this_thread::get_id(); }
};
}
#endif
