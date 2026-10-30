#ifndef OUTSHINE_RENDER_SCENE_SCENERESOURCES_H
#define OUTSHINE_RENDER_SCENE_SCENERESOURCES_H

#include "ResourceHandle.h"
#include "HeightPageBatch.h"
#include "PieceMesh.h"
#include "GroundTile.h"
#include "MaterialBinding.h"
#include "MaterialSource.h"
#include "MaterialRegistry.h"
#include "TerrainTile.h"
#include "PrismInstance.h"
#include <PrismGrid.h>
#include "device/native/Commands.h"
#include "Geometry.h"

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {

class GeometryRenderer;

struct GroundClassificationSource {
  std::shared_ptr<const uint32_t> Classes;
  size_t ClassWords = 0;
  std::shared_ptr<const float> Palette;
  size_t PaletteFloats = 0;

  [[nodiscard]] std::span<const uint32_t> ClassSpan() const noexcept {
    return {Classes.get(), ClassWords};
  }

  [[nodiscard]] std::span<const float> PaletteSpan() const noexcept {
    return {Palette.get(), PaletteFloats};
  }
};

class SceneResources {
public:
  enum class PieceSources : uint8_t { Copy, Omit };
  enum class PieceOwner : uint8_t { Independent, StructureTile };

  struct PieceRows {
    PieceHandle Piece;
    std::span<const Mat4> Rows;
  };

  [[nodiscard]] std::expected<PieceHandle, std::string>
  PlacePiece(GeometryRenderer &geometry,
             const PieceMesh &piece,
             PieceOwner owner = PieceOwner::Independent);
  [[nodiscard]] std::expected<size_t, std::string>
  ReconcileStructurePieces(GeometryRenderer &geometry, std::span<const PieceHandle> held);
  [[nodiscard]] bool SetPiecePasses(GeometryRenderer &geometry,
                                    PieceHandle which,
                                    GeometryPasses passes,
                                    std::string &error);
  [[nodiscard]] bool SetPieceInstances(GeometryRenderer &geometry,
                                       PieceHandle which,
                                       std::span<const Mat4> rows,
                                       std::string &error);
  [[nodiscard]] bool SetPieceInstances(GeometryRenderer &geometry,
                                       std::span<const PieceRows> pieces,
                                       std::string &error);
  void ReleasePiece(GeometryRenderer &geometry, PieceHandle which);

  void CopySourcesFrom(const SceneResources &source, PieceSources pieces);
  [[nodiscard]] bool RestorePieces(GeometryRenderer &geometry, std::string &error);
  [[nodiscard]] std::expected<bool, std::string>
  AdvancePieceRestore(GeometryRenderer &geometry, size_t &nextPiece, size_t piecesMost);

  [[nodiscard]] std::expected<MaterialRegistration, std::string>
  RegisterPieceMaterials(GeometryRenderer &geometry,
                         GeometryRenderer *glass,
                         Geometry source,
                         std::span<const MaterialRadiance> images = {});
  [[nodiscard]] bool
  RestorePieceMaterials(GeometryRenderer &geometry, GeometryRenderer *glass, std::string &error);
  [[nodiscard]] bool
  PreparePieceMaterials(GeometryRenderer &geometry, GeometryRenderer *glass, std::string &error);

  [[nodiscard]] size_t PieceSourceBytes() const noexcept;
  [[nodiscard]] size_t PieceSourceCount() const noexcept;
  [[nodiscard]] bool HasPiece(PieceHandle handle) const noexcept;

  [[nodiscard]] Native::Result<PrismGridHandle>
  PlacePrismGrid(GeometryRenderer &geometry,
                 std::shared_ptr<const PrismGrid> source,
                 std::span<const PrismInstance> instances,
                 const MaterialRegistration &material);
  [[nodiscard]] Native::Result<void> ReplacePrismGrid(GeometryRenderer &geometry,
                                                      PrismGridHandle which,
                                                      std::shared_ptr<const PrismGrid> source,
                                                      std::span<const PrismInstance> instances,
                                                      const MaterialRegistration &material);
  [[nodiscard]] Native::Result<void> SetPrismGridInstances(
      GeometryRenderer &geometry, PrismGridHandle which, std::span<const PrismInstance> instances);
  void ReleasePrismGrid(GeometryRenderer &geometry, PrismGridHandle which);
  [[nodiscard]] bool HasPrismGrid(PrismGridHandle which) const noexcept;

  [[nodiscard]] size_t PieceSlots() const noexcept { return Pieces_.size(); }

  [[nodiscard]] size_t PieceSlotBytes() const noexcept {
    return Pieces_.capacity() * sizeof(Piece);
  }

  [[nodiscard]] Native::Result<HeightPageHandle> PlaceHeightPage(GeometryRenderer &geometry,
                                                                 std::span<const float> nodes);
  [[nodiscard]] Native::Result<std::vector<HeightPageHandle>>
  PlaceHeightPages(GeometryRenderer &geometry, std::span<const HeightPageView> nodes);
  void ReleaseHeightPage(GeometryRenderer &geometry, HeightPageHandle which);
  [[nodiscard]] bool HasHeightPage(HeightPageHandle handle) const noexcept;
  [[nodiscard]] PageId HeightPageResident(HeightPageHandle which) const noexcept;
  [[nodiscard]] std::expected<bool, std::string>
  AdvanceTerrainRestore(GeometryRenderer &geometry, size_t &nextPage, size_t pagesMost);
  [[nodiscard]] Native::Result<void> SetGroundGrid(GeometryRenderer &geometry,
                                                   std::span<const float> fractions);
  [[nodiscard]] Native::Result<void> SetTerrainTiles(GeometryRenderer &geometry,
                                                     std::span<const TerrainTile> real,
                                                     std::span<const TerrainTile> virtual_);
  [[nodiscard]] bool RestoreTerrain(GeometryRenderer &geometry, std::string &error);
  void SetGroundClassification(std::span<const uint32_t> classes, std::span<const float> palette);
  void SetGroundClassification(GroundClassificationSource source) noexcept;

  [[nodiscard]] std::span<const uint32_t> GroundClasses() const noexcept {
    return GroundClassification_.ClassSpan();
  }

  [[nodiscard]] std::span<const float> GroundPalette() const noexcept {
    return GroundClassification_.PaletteSpan();
  }

  [[nodiscard]] size_t HeightPageSourceBytes() const noexcept;

  [[nodiscard]] size_t HeightPageSlots() const noexcept { return HeightPages_.size(); }

  [[nodiscard]] size_t HeightPageSlotBytes() const noexcept {
    return HeightPages_.capacity() * sizeof(HeightPage);
  }

private:
  struct PieceSource {
    std::vector<float> Tangents;
    std::vector<StoredVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<DagCluster> Clusters;
    std::vector<float> Colours;
    std::vector<float> Uv1;
  };

  struct Piece {
    ResourceSlotState State{};
    std::shared_ptr<const PieceSource> Source;
    Mat4 Row;
    std::vector<Mat4> Rows;
    uint32_t MaxInstances = 0;
    PieceSurface Surface;
    PieceId Resident = kNoPiece;
    PieceOwner Owner = PieceOwner::Independent;
    bool Textured = false;
    std::array<uint64_t, 3> ProductOrder{};
    GeometryPasses Passes = GeometryPasses::MainAndShadow;

    [[nodiscard]] PieceMesh Mesh() const noexcept;
  };

  [[nodiscard]] bool HasMaterialRegistration(const MaterialRegistration &material) const noexcept;

  struct HeightPage {
    ResourceSlotState State{};
    std::shared_ptr<const std::vector<float>> Nodes;
    PageId Resident = kNoPage;
  };

  struct PrismSource {
    ResourceSlotState State{};
    std::shared_ptr<const PrismGrid> Source;
    std::vector<PrismInstance> Instances;
    MaterialRegistration Material;
    uint64_t Resident = 0;
  };

  void CopyPrismSourcesFrom(const SceneResources &source, PieceSources pieces);
  [[nodiscard]] std::expected<bool, std::string>
  AdvancePrismRestore(GeometryRenderer &geometry, size_t &nextGrid, size_t gridsMost);

  [[nodiscard]] Native::Result<size_t>
  RestoreHeightPageBatch(GeometryRenderer &geometry, size_t first, size_t pagesMost);

  std::vector<Piece> Pieces_;
  uint32_t FirstFreePiece_ = kNoResourceSlot;
  MaterialRegistry Materials_;
  std::vector<HeightPage> HeightPages_;
  uint32_t FirstFreeHeightPage_ = kNoResourceSlot;
  std::vector<PrismSource> PrismGrids_;
  uint32_t FirstFreePrismGrid_ = kNoResourceSlot;
  std::vector<float> GroundGrid_;
  std::vector<TerrainTile> GroundReal_;
  std::vector<TerrainTile> GroundVirtual_;
  GroundClassificationSource GroundClassification_;
};

}
#endif
