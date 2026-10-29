#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDPASS_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDPASS_H
#include "MaterialFieldInput.h"
#include "RadianceImage.h"
#include "SceneLighting.h"
#include "Viewing.h"
#include <atomic>
#include <memory>
#include <optional>
#include <span>

namespace outshine::Render {
class UploadQueue;

class ResidentMaterialField {
public:
  ~ResidentMaterialField();
  [[nodiscard]] Native::Completion Produced() const noexcept;
  ResidentMaterialField(const ResidentMaterialField &) = delete;
  ResidentMaterialField &operator=(const ResidentMaterialField &) = delete;

private:
  friend class MaterialFieldPass;
  struct Data;
  explicit ResidentMaterialField(std::shared_ptr<Data> data) noexcept;
  std::shared_ptr<Data> Data_;
};

struct MaterialFieldView {
  Viewpoint Camera;
  std::span<const SceneLight> Lights;
  EnvironmentLighting Environment;
  Extent Target;
};

struct MaterialFieldImages {
  std::shared_ptr<const RadianceImage> Unfiltered;
  std::shared_ptr<const RadianceImage> Filtered;
  // Raw depth shares Unfiltered's raster/completion; it is not filtered depth or occlusion
  // authority.
  std::shared_ptr<Native::Texture> Depth;
  Viewpoint Camera;
};

class MaterialFieldPass {
public:
  // Device outlives this pass and its GPU leases. Calls and GPU lease releases use its owner
  // thread.
  explicit MaterialFieldPass(Native::Device &device, UploadQueue *uploads = nullptr);
  ~MaterialFieldPass();
  MaterialFieldPass(const MaterialFieldPass &) = delete;
  MaterialFieldPass &operator=(const MaterialFieldPass &) = delete;

  // Explicit device/pipeline preparation, outside the frame's admitted relighting work.
  [[nodiscard]] Native::Result<void> Configure();
  [[nodiscard]] Native::Result<std::shared_ptr<const ResidentMaterialField>>
  Prepare(const std::shared_ptr<const MaterialFieldInput> &input,
          const std::atomic_bool *stopping = nullptr);
  [[nodiscard]] Native::Result<MaterialFieldImages>
  Relight(const ResidentMaterialField &source,
          const MaterialFieldView &view,
          const std::atomic_bool *stopping = nullptr);
  [[nodiscard]] Native::Result<void> Collect();
  [[nodiscard]] std::optional<Native::Completion> LastSubmission() const noexcept;

private:
  struct State;
  std::unique_ptr<State> State_;
};
}
#endif
