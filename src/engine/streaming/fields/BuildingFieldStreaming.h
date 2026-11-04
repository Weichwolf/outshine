#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMING_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMING_H
#include "TangentFrame.h"
#include "Viewing.h"
#include <cstddef>
#include <memory>
#include <string>

namespace outshine::Ground {
class SurfacePreparation;
}

namespace outshine::Render {
class GeometryPageResidency;
class SceneRenderer;
}

namespace outshine {
class Tasks;
class AssetPipeline;
class StructureBuildQueue;

class BuildingFieldStreaming {
  struct State;

public:
  struct Context {
    Tasks &Workers;
    AssetPipeline &Pipeline;
    Ground::SurfacePreparation &Surface;
    StructureBuildQueue &Sources;
    Render::GeometryPageResidency &Pages;
    Render::SceneRenderer &Renderer;
    Render::Viewpoint Camera;
    TangentFrame Frame;
    LongitudeLatitude Eye;
  };

  struct Counts {
    size_t Sources = 0, Pending = 0, Ready = 0, Active = 0, Rejected = 0;
    size_t ReadyGroups = 0, ActiveGroups = 0;
  };

  BuildingFieldStreaming();
  ~BuildingFieldStreaming();
  BuildingFieldStreaming(const BuildingFieldStreaming &) = delete;
  BuildingFieldStreaming &operator=(const BuildingFieldStreaming &) = delete;
  [[nodiscard]] bool Advance(Context &context, std::string &error);
  [[nodiscard]] Counts Read() const noexcept;

private:
  std::unique_ptr<State> State_;
};
}
#endif
