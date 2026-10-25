#ifndef OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#define OUTSHINE_GENERATORS_EARTH_TERRAININPUTPREPARATION_H
#include "GroundMesher.h"
#include "HeightField.h"
#include "TerrainLoader.h"
#include "TerrainFieldSet.h"
#include "TerrainRefinement.h"
#include "TangentFrame.h"
#include <cstddef>
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
    bool BuildingFootprints = true;
    std::span<const Data::TileId> AdditionalTiles;
  };

  class HaloBuildJob {
  public:
    HaloBuildJob(TerrainInputPreparation &inputs, Patchwork &candidate, int finestZoom) noexcept;

    [[nodiscard]] bool Advance(size_t nodesMost);

    [[nodiscard]] size_t Haloed() const noexcept { return Haloed_; }

  private:
    [[nodiscard]] bool BeginsSheet();
    [[nodiscard]] bool AdvancesNode();
    void CompletesSheet();

    TerrainInputPreparation *Inputs_ = nullptr;
    Patchwork *Candidate_ = nullptr;
    int FinestZoom_ = 0;
    size_t SheetAt_ = 0;
    size_t NodeAt_ = 0;
    size_t Haloed_ = 0;
    int Zoom_ = 0;
    double Span_ = 0;
    double AtX_ = 0;
    double AtY_ = 0;
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
                FieldPreparation preparation);

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
                  std::span<const TerrainRefinementCorridor> corridors = {}) const;
  [[nodiscard]] size_t Halos(Patchwork &laid, int finestZoom);

  [[nodiscard]] std::optional<double> FieldUpM(int zoom, EastNorth at) const;

  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;

  [[nodiscard]] const ::outshine::Ground::TerrainField *FieldAt(Data::TileId tile) const;
  [[nodiscard]] bool ShareSourcedField(Data::TileId tile,
                                       ::outshine::Ground::HeightField::Block &into) const;
  [[nodiscard]] TerrainFieldSet SnapshotSourceFields() const;

  void Reset() {
    Fields_.clear();
    Metadata_.reset();
    Requests_.clear();
    NextRequest_ = 0;
    ResolvedRequests_ = 0;
    RequestsPrepared_ = false;
  }

  [[nodiscard]] size_t RimsMissing() const noexcept { return RimsMissing_; }

  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  [[nodiscard]] std::optional<float> AslAt(int zoom, ::outshine::Ground::TileFrac at) const;

  struct FieldRequest {
    Data::TileId Tile;
    bool Resolved = false;
  };

  ::outshine::Ground::TerrainRevisionIndex::Reservation Metadata_;
  std::vector<std::pair<Data::TileId, std::shared_ptr<const ::outshine::Ground::TerrainField>>>
      Fields_;
  std::vector<FieldRequest> Requests_;
  size_t NextRequest_ = 0;
  size_t ResolvedRequests_ = 0;
  bool RequestsPrepared_ = false;
  size_t RimsMissing_ = 0;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};
}
#endif
