#ifndef OUTSHINE_RENDER_SCENE_SCENERESOURCES_H
#define OUTSHINE_RENDER_SCENE_SCENERESOURCES_H

#include "ResourceHandle.h"
#include "SubjectTypes.h"
#include "TerrainTile.h"
#include "scene/Geometry.h"

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
  PlacePiece(GeometryRenderer &subjects,
             const PieceMesh &piece,
             PieceOwner owner = PieceOwner::Independent);
  [[nodiscard]] std::expected<size_t, std::string>
  ReconcileStructurePieces(GeometryRenderer &subjects, std::span<const PieceHandle> held);
  [[nodiscard]] bool SetPieceInstances(GeometryRenderer &subjects,
                                       PieceHandle which,
                                       std::span<const Mat4> rows,
                                       std::string &error);
  [[nodiscard]] bool SetPieceInstances(GeometryRenderer &subjects,
                                       std::span<const PieceRows> pieces,
                                       std::string &error);
  void ReleasePiece(GeometryRenderer &subjects, PieceHandle which);

  void CopySourcesFrom(const SceneResources &source, PieceSources pieces);
  [[nodiscard]] bool RestorePieces(GeometryRenderer &subjects, std::string &error);
  [[nodiscard]] std::expected<bool, std::string>
  AdvancePieceRestore(GeometryRenderer &subjects, size_t &nextPiece, size_t piecesMost);

  [[nodiscard]] std::expected<uint32_t, std::string>
  RegisterPieceMaterials(GeometryRenderer &subjects, GeometryRenderer *glass, Geometry source);
  [[nodiscard]] bool
  RestorePieceMaterials(GeometryRenderer &subjects, GeometryRenderer *glass, std::string &error);

  [[nodiscard]] size_t PieceSourceBytes() const noexcept;
  [[nodiscard]] size_t PieceSourceCount() const noexcept;
  [[nodiscard]] bool HasPiece(PieceHandle handle) const noexcept;

  [[nodiscard]] size_t PieceSlots() const noexcept { return Pieces_.size(); }

  [[nodiscard]] size_t PieceSlotBytes() const noexcept {
    return Pieces_.capacity() * sizeof(Piece);
  }

  [[nodiscard]] std::expected<HeightPageHandle, std::string>
  PlaceHeightPage(GeometryRenderer &subjects, std::span<const float> nodes);
  void ReleaseHeightPage(GeometryRenderer &subjects, HeightPageHandle which);
  [[nodiscard]] bool HasHeightPage(HeightPageHandle handle) const noexcept;
  [[nodiscard]] PageId HeightPageResident(HeightPageHandle which) const noexcept;
  [[nodiscard]] bool RestoreHeightPages(GeometryRenderer &subjects, std::string &error);
  [[nodiscard]] std::expected<bool, std::string>
  AdvanceTerrainRestore(GeometryRenderer &subjects, size_t &nextPage, size_t pagesMost);
  [[nodiscard]] bool
  SetGroundGrid(GeometryRenderer &subjects, std::span<const float> fractions, std::string &error);
  [[nodiscard]] bool SetTerrainTiles(GeometryRenderer &subjects,
                                     std::span<const TerrainTile> real,
                                     std::span<const TerrainTile> virtual_,
                                     std::string &error);
  [[nodiscard]] bool RestoreTerrain(GeometryRenderer &subjects, std::string &error);
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

    [[nodiscard]] PieceMesh Mesh() const noexcept;
  };

  struct PieceMaterials {
    Geometry Source;
    std::vector<SubjectMaterial> Slots;
  };

  [[nodiscard]] static std::expected<PieceMaterials, std::string>
  ResolvePieceMaterials(Geometry source);
  [[nodiscard]] bool AppendPieceMaterials(GeometryRenderer &subjects,
                                          GeometryRenderer *glass,
                                          const PieceMaterials &materials,
                                          std::string &error);

  struct HeightPage {
    ResourceSlotState State{};
    std::shared_ptr<const std::vector<float>> Nodes;
    PageId Resident = kNoPage;
  };

  std::vector<Piece> Pieces_;
  uint32_t FirstFreePiece_ = kNoResourceSlot;
  std::vector<std::shared_ptr<const PieceMaterials>> PieceMaterials_;
  std::vector<uint32_t> RegisteredPieceSlots_;
  std::vector<HeightPage> HeightPages_;
  uint32_t FirstFreeHeightPage_ = kNoResourceSlot;
  std::vector<float> GroundGrid_;
  std::vector<TerrainTile> GroundReal_;
  std::vector<TerrainTile> GroundVirtual_;
  GroundClassificationSource GroundClassification_;
};

}
#endif
