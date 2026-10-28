#ifndef OUTSHINE_ENGINE_STREAMING_BUILDINGSOURCEDEMAND_H
#define OUTSHINE_ENGINE_STREAMING_BUILDINGSOURCEDEMAND_H
#include "Box.h"
#include "Vec3.h"
#include "ShapedGround.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
class PreparedBuildingAssets;

namespace Osm {
class OsmField;
}
}

namespace outshine::Ground {
class SurfacePreparation;
}

namespace outshine {
class StructureBuildQueue;

class BuildingSourceDemand {
public:
  struct Source {
    std::string RequestKey;
    std::string BaseKey;
    Box Bounds;
    Vec3 AnchorEcef;
    uint64_t Cells = 0;
    bool HeightQualified = false;
  };
  enum class Phase : uint8_t { Unrequested, Pending, Missing, Ready, Failed };

  struct Entry {
    Phase Where = Phase::Unrequested;
    std::optional<Source> Value;
  };

  struct Summary {
    size_t Ready = 0, Pending = 0, Missing = 0, Failed = 0;
    bool operator==(const Summary &) const = default;
  };

  [[nodiscard]] bool
  Advance(StructureBuildQueue &queue, Ground::SurfacePreparation &stack, size_t requestsMost);

  [[nodiscard]] std::span<const Entry> Entries() const noexcept { return Entries_; }

  [[nodiscard]] Summary Counts() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  std::vector<Entry> Entries_;
  std::weak_ptr<Generators::PreparedBuildingAssets> Assets_;
  const Generators::Osm::OsmField *Vectors_ = nullptr;
  uint64_t VectorGeneration_ = 0, BasisWrites_ = 0;
  Ground::ShapedGround Shape_;
  double TileSpanM_ = 0;
  size_t Next_ = 0;
};
}
#endif
