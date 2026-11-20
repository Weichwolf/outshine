#ifndef OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#define OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#include "GroundMesher.h"
#include "HeightField.h"
#include "TerrainLoader.h"
#include "TerrainFieldSet.h"
#include "TerrainRefinement.h"
#include "TangentFrame.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
class OsmField;
}

namespace outshine::Generators {
class TerrainHaloCache {
public:
  static constexpr size_t kSourceWidth = 3;
  using SourceFields = std::array<std::shared_ptr<const ::outshine::Ground::TerrainField>,
                                  kSourceWidth * kSourceWidth>;

  void SelectWindow(uint64_t generation, std::span<const Sheet> sheets);
  [[nodiscard]] bool
  Restore(uint64_t generation, Sheet &sheet, int sourceZoom, const SourceFields &fields);
  void Retain(uint64_t generation, const Sheet &sheet, int sourceZoom, const SourceFields &fields);

  [[nodiscard]] size_t Hits() const noexcept { return Hits_.load(std::memory_order_relaxed); }

private:
  struct Page {
    std::vector<float> Nodes;
    std::array<std::weak_ptr<const ::outshine::Ground::TerrainField>, kSourceWidth * kSourceWidth>
        Fields;
    int SourceZoom = -1;
  };

  std::mutex Mutex_;
  std::map<std::tuple<int, uint32_t, uint32_t>, Page> Pages_;
  uint64_t Window_ = 0;
  std::atomic_size_t Hits_{0};
};

class TerrainInputPreparation {
public:
  struct FieldPreparation {
    int FinestZoom;
    size_t RequestsMost;
    const ::outshine::Generators::Osm::OsmField *Vectors = nullptr;
    std::span<const Data::TileId> AdditionalTiles;
  };

  class HaloBuildJob {
  public:
    HaloBuildJob(TerrainInputPreparation &inputs,
                 Patchwork &candidate,
                 int finestZoom,
                 TerrainHaloCache *cache = nullptr,
                 uint64_t generation = 0) noexcept;

    [[nodiscard]] bool Advance(size_t nodesMost);

    [[nodiscard]] size_t Haloed() const noexcept { return Haloed_; }

  private:
    static constexpr size_t kPageWidth = kPatchGrid + 3u;
    static constexpr size_t kNeighbourWidth = TerrainHaloCache::kSourceWidth;

    struct AxisSample {
      size_t Field = 0;
      double Fraction = 0.0;
    };

    [[nodiscard]] static std::array<AxisSample, kPageWidth>
    SampleAxis(const Sheet &sheet, uint32_t coordinate, double span);
    void LoadNeighbours(Data::TileId source);
    [[nodiscard]] bool PrepareSampling(Sheet &sheet);
    [[nodiscard]] bool BeginsSheet();
    [[nodiscard]] bool AdvancesNode();
    void CompletesSheet();

    TerrainInputPreparation *Inputs_ = nullptr;
    Patchwork *Candidate_ = nullptr;
    int FinestZoom_ = 0;
    TerrainHaloCache *Cache_ = nullptr;
    uint64_t Generation_ = 0;
    size_t SheetAt_ = 0;
    size_t NodeAt_ = 0;
    size_t Haloed_ = 0;
    std::optional<Data::TileId> SamplingSource_;
    TerrainHaloCache::SourceFields SamplingFields_{};
    std::array<AxisSample, kPageWidth> Columns_{}, Rows_{};
    std::vector<float> Page_;
    std::vector<bool> Missing_;
    bool Whole_ = false;
    bool AnyMissing_ = false;
    bool Working_ = false;
  };

  void Framed(const TangentFrame &frame) {
    Frame_ = frame;
    Framed_ = true;
  }

  [[nodiscard]] std::expected<bool, std::string>
  PrepareFields(const Patchwork &candidate,
                const ::outshine::Ground::GroundStream &ground,
                FieldPreparation preparation,
                std::unique_ptr<TerrainFieldSet> previous = {});

  [[nodiscard]] std::string FieldDiagnostic() const;

  [[nodiscard]] TerrainRefinementJob
  BeginRefinement(const Patchwork &candidate,
                  TerrainPageLayout layout,
                  TerrainRefinementDetail detail,
                  size_t maximumPatches,
                  std::span<const TerrainRefinementCorridor> corridors = {},
                  TerrainRefinementCache *cache = nullptr) const;
  [[nodiscard]] size_t Halos(Patchwork &laid, int finestZoom);

  [[nodiscard]] std::optional<double> FieldUpM(int zoom, EastNorth at) const;

  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;

  [[nodiscard]] const ::outshine::Ground::TerrainField *FieldAt(Data::TileId tile) const;
  [[nodiscard]] bool ShareExactSourceField(Data::TileId tile,
                                           ::outshine::Ground::HeightField::Block &into) const;
  [[nodiscard]] TerrainFieldSet SnapshotSourceFields() const;

  void Reset() {
    Fields_.clear();
    FieldHeapBytes_ = 0;
    Metadata_.reset();
    Requests_.clear();
    NextRequest_ = 0;
    FieldsWanted_ = 0;
    RequestsPrepared_ = false;
    TerrainScope_ = 0;
  }

  [[nodiscard]] size_t RimsMissing() const noexcept { return RimsMissing_; }

  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  [[nodiscard]] const TerrainFieldSet::Entry *FindField(Data::TileId tile) const;
  [[nodiscard]] std::expected<void, std::string>
  BeginFieldRequests(const Patchwork &candidate,
                     const ::outshine::Ground::GroundStream &ground,
                     FieldPreparation preparation,
                     std::unique_ptr<TerrainFieldSet> previous);

  struct FieldRequest {
    Data::TileId Tile;
    bool Resolved = false;
  };

  void ResolveField(FieldRequest &request,
                    std::shared_ptr<const ::outshine::Ground::TerrainField> field);
  void ReuseFields(const TerrainFieldSet &previous, const ::outshine::Ground::GroundStream &ground);

  ::outshine::Ground::TerrainRevisionIndex::Reservation Metadata_;
  std::vector<std::pair<Data::TileId, std::shared_ptr<const ::outshine::Ground::TerrainField>>>
      Fields_;
  size_t FieldHeapBytes_ = 0;
  std::vector<FieldRequest> Requests_;
  size_t NextRequest_ = 0;
  size_t FieldsWanted_ = 0;
  bool RequestsPrepared_ = false;
  uint64_t TerrainScope_ = 0;
  size_t RimsMissing_ = 0;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};
}
#endif
