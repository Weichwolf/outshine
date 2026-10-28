#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORBAKER_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORBAKER_H

#include "ImpostorAtlas.h"
#include "ImpostorAtlasShape.h"
#include "Mat4.h"
#include "Vec3.h"
#include "Geometry.h"
#include "GpuSubmission.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {
class SceneRenderer;
class ExecutionPlan;

struct ImpostorCaptureSource {
  Geometry Mesh;
  std::vector<Mat4> Instances;
};

struct ImpostorCapture {
  std::vector<ImpostorCaptureSource> Sources;
  Vec3 LeastM;
  Vec3 MostM;
  std::vector<Vec3> ViewDirections;
};

class ImpostorBaker {
public:
  explicit ImpostorBaker(GpuSubmission submission = {});
  ~ImpostorBaker();
  ImpostorBaker(const ImpostorBaker &) = delete;
  ImpostorBaker &operator=(const ImpostorBaker &) = delete;

  // Serialize captures on a content worker; this renderer never belongs to the frame loop.
  [[nodiscard]] std::optional<ImpostorAtlas>
  Capture(ImpostorCapture capture, ImpostorAtlasShape shape, std::string &error);

  [[nodiscard]] static std::optional<ImpostorAtlas>
  Bake(ImpostorCapture capture, ImpostorAtlasShape shape, std::string &error);

private:
  [[nodiscard]] bool Prepare(int pixels, std::string &error);

  std::unique_ptr<SceneRenderer> Renderer_;
  std::shared_ptr<const ExecutionPlan> Plan_;
  int Pixels_ = 0;
};

}
#endif
