#ifndef OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#define OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#include "GroundMesher.h"
#include "HeightField.h"
#include "TerrainLoader.h"
#include "TerrainFieldSet.h"
#include "TerrainRefinement.h"
#include "TangentFrame.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
class OsmField;
}

namespace outshine::Generators {
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
    HaloBuildJob(TerrainInputPreparation &inputs, Patchwork &candidate, int finestZoom) noexcept;

    [[nodiscard]] bool Advance(size_t nodesMost);

    [[nodiscard]] size_t Haloed() const noexcept { return Haloed_; }

  private:
    static constexpr size_t kPageWidth = kPatchGrid + 3u;
    static constexpr size_t kNeighbourWidth = 3u;

    struct AxisSample {
      size_t Field = 0;
      double Fraction = 0.0;
    };

    [[nodiscard]] static std::array<AxisSample, kPageWidth>
    SampleAxis(const Sheet &sheet, uint32_t coordinate, double span);
    void LoadNeighbours(Data::TileId source);
    void PrepareSampling(const Sheet &sheet);
    [[nodiscard]] bool BeginsSheet();
    [[nodiscard]] bool AdvancesNode();
    void CompletesSheet();

    TerrainInputPreparation *Inputs_ = nullptr;
    Patchwork *Candidate_ = nullptr;
    int FinestZoom_ = 0;
    size_t SheetAt_ = 0;
    size_t NodeAt_ = 0;
    size_t Haloed_ = 0;
    std::optional<Data::TileId> SamplingSource_;
    std::array<std::shared_ptr<const ::outshine::Ground::TerrainField>,
               kNeighbourWidth * kNeighbourWidth>
        SamplingFields_{};
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

  [[nodiscard]] bool RefineByError(Patchwork &candidate,
                                   TerrainPageLayout layout,
                                   TerrainRefinementDetail detail,
                                   size_t maximumPatches,
                                   std::string &error) const;
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
