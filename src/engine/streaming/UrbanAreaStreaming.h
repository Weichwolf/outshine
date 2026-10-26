#ifndef OUTSHINE_ENGINE_STREAMING_URBANAREASTREAMING_H
#define OUTSHINE_ENGINE_STREAMING_URBANAREASTREAMING_H

#include "PreparedUrbanAreas.h"
#include "SourceReader.h"
#include "PrismGridViews.h"
#include "ResourceHandle.h"
#include "TangentFrame.h"
#include "UrbanAreaGroups.h"
#include "UrbanPrismSlot.h"
#include "TileGeodesy.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
class TerrainResidency;

namespace Render {
class SceneRenderer;
struct PrismInstance;
}

class UrbanAreaStreaming {
public:
  struct View {
    LongitudeLatitude Focus;
    Vec3 EyeEcef;
    double FocalPx = 0, RadiusM = 0;
    TangentFrame Frame = TangentFrame::At({});
  };

  struct Counters {
    size_t Sources = 0, Groups = 0, Resident = 0, InFlight = 0, Missing = 0;
    uint64_t Captures = 0, Publications = 0;
  };

  UrbanAreaStreaming(Tasks &pool,
                     std::shared_ptr<Generators::Osm::PreparedUrbanAreas> sources,
                     Data::SourceReader &reader,
                     Generators::Osm::MvtSchema schema,
                     std::string database);
  ~UrbanAreaStreaming();
  void Into(Render::SceneRenderer &renderer) noexcept;
  [[nodiscard]] std::expected<void, std::string>
  Advance(const View &view,
          const TerrainResidency &terrain,
          uint64_t terrainRevision,
          std::span<const Ground::GeoBounds> children,
          uint64_t childrenRevision);
  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;
  [[nodiscard]] Counters Costs() const noexcept;

private:
  struct Source {
    Data::TileId Owner;
    Generators::Osm::PreparedUrbanAreas::Request Request;
    Generators::Osm::PreparedUrbanAreas::SourceDelivery SourceDelivery;
    std::vector<size_t> OccupiedGroups;
  };

  struct Group {
    Data::TileId Owner;
    size_t Index = 0;
    std::shared_ptr<const Generators::Osm::UrbanArea> Area;
    std::vector<Generators::Osm::UrbanPatchRange> Patches;
    std::string SourceKey, Request, PublishedKey;
    std::unique_ptr<UrbanPrismSlot> Slot;
    Render::PrismGridHandle Handle;
    std::vector<PrismCellRect> Exclusions;
    uint64_t ChildrenRevision = 0;
  };

  struct Candidate {
    size_t Source = 0, Group = 0;
    double DistanceM = 0;
  };

  [[nodiscard]] std::expected<std::vector<Candidate>, std::string>
  SelectGroups(const View &view, const TerrainResidency &terrain) const;
  [[nodiscard]] std::expected<void, std::string> PlanSources(const View &view);
  [[nodiscard]] std::expected<bool, std::string> PollSource();
  [[nodiscard]] std::expected<void, std::string>
  AdvanceGroups(const View &view,
                const TerrainResidency &terrain,
                std::span<const Ground::GeoBounds> children,
                uint64_t childrenRevision);
  [[nodiscard]] std::expected<bool, std::string>
  Upload(Group &group,
         const std::shared_ptr<const PrismGrid> &geometry,
         std::span<const Render::PrismInstance> instances);
  [[nodiscard]] std::expected<void, std::string> PlanGroups(const View &view,
                                                            const TerrainResidency &terrain);
  [[nodiscard]] std::expected<bool, std::string>
  Publish(Group &group,
          const View &view,
          std::span<const Ground::GeoBounds> children,
          uint64_t childrenRevision);
  void Retire(Group &group);
  Tasks &Pool_;
  std::shared_ptr<Generators::Osm::PreparedUrbanAreas> Sources_;
  Data::SourceReader &Reader_;
  Generators::Osm::MvtSchema Schema_;
  std::string Database_;
  Render::SceneRenderer *Renderer_ = nullptr;
  std::optional<uint32_t> Material_;
  std::optional<View> View_;
  std::optional<uint64_t> TerrainRevision_;
  std::vector<Source> Areas_;
  std::vector<Group> Groups_;
  std::vector<std::unique_ptr<UrbanPrismSlot>> Retiring_;
  size_t NextSource_ = 0;
  uint64_t Captures_ = 0, Publications_ = 0;
  uint64_t WantedChildrenRevision_ = 0;
  bool Planned_ = false;
};
}
#endif
