#ifndef OUTSHINE_ENGINE_STREAMING_BUILDINGFIELDREQUEST_H
#define OUTSHINE_ENGINE_STREAMING_BUILDINGFIELDREQUEST_H
#include "AssetPipeline.h"
#include "BuildingSourceDemand.h"
#include "BuildingSourceBudget.h"
#include "ImpostorAtlas.h"
#include "ImpostorViewAssets.h"
#include "ImpostorPoseBounds.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine {
namespace Render {
class MaterialFieldInput;
}

class BuildingFieldRequest {
  struct State;

public:
  enum class CoordinateSpace : uint8_t { CaptureLocal, World };

  struct Pose {
    // Snapshot only; movement requires a fresh geometric evaluation before replacement.
    Render::CameraBasis Camera;
    Render::Lens Optics;
    double GeometryErrorMostPx = 0;
    CoordinateSpace Space = CoordinateSpace::CaptureLocal;
  };

  struct Demand {
    std::vector<Vec3> Directions;
    uint32_t Pixels = 128;
    Content::ImpostorViewAssets::ViewIndex View;
    // Geometric selection only; an absent pose is a raw diagnostic request.
    std::optional<Pose> GeometricView = std::nullopt;
  };

  struct Budget {
    size_t ProducerBytes = 0;
    size_t ResidentBytes = 0;
    size_t FieldBytesMost = 0;
    // Additional CPU raster reservation, retained with the field publication.
    size_t RasterBytesMost = 0;
    Generators::BuildingSourceBudget Capture;
  };

  struct Product {
    BuildingSourceDemand::Source Source;
    std::shared_ptr<const Content::ImpostorViewAssets> Metadata;
    std::shared_ptr<const ImpostorAtlas> Field;
    std::shared_ptr<const Render::MaterialFieldInput> RenderInput;
    std::optional<Render::ImpostorPoseBounds::Error> GeometricError = std::nullopt;
  };

  // Pipeline and workers outlive the request and its returned CPU leases.
  [[nodiscard]] static std::expected<BuildingFieldRequest, AssetPublicationError>
  Start(AssetPipeline &pipeline,
        Tasks &workers,
        const std::shared_ptr<AssetCache> &cache,
        std::shared_ptr<Generators::PreparedBuildingAssets> sources,
        BuildingSourceDemand::Source source,
        const Demand &demand,
        Budget budget);
  BuildingFieldRequest(BuildingFieldRequest &&) noexcept;
  BuildingFieldRequest &operator=(BuildingFieldRequest &&other) noexcept;
  ~BuildingFieldRequest();
  BuildingFieldRequest(const BuildingFieldRequest &) = delete;
  BuildingFieldRequest &operator=(const BuildingFieldRequest &) = delete;
  [[nodiscard]] std::expected<std::shared_ptr<const Product>, std::string> Poll();
  void Cancel() noexcept;

private:
  explicit BuildingFieldRequest(std::shared_ptr<State> state) noexcept;
  std::shared_ptr<State> State_;
};
}
#endif
