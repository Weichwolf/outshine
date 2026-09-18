#ifndef OUTSHINE_RENDER_SCENE_SCENERESOURCES_H
#define OUTSHINE_RENDER_SCENE_SCENERESOURCES_H

#include "ResourceHandle.h"
#include "SubjectTypes.h"
#include "TerrainTile.h"
#include "scene/Geometry.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {

class SubjectDraw;

class SceneResources {
public:
  struct PieceRows {
    PieceHandle Piece;
    std::span<const Mat4> Rows;
  };

  [[nodiscard]] std::expected<PieceHandle, std::string> PlacePiece(SubjectDraw &subjects,
                                                                   const PieceMesh &piece);
  [[nodiscard]] bool SetPieceInstances(SubjectDraw &subjects,
                                       PieceHandle which,
                                       std::span<const Mat4> rows,
                                       std::string &error);
  [[nodiscard]] bool
  SetPieceInstances(SubjectDraw &subjects, std::span<const PieceRows> pieces, std::string &error);
  void ReleasePiece(SubjectDraw &subjects, PieceHandle which);

  [[nodiscard]] bool CopySourcesFrom(const SceneResources &source, std::string &error);
  [[nodiscard]] bool RestorePieces(SubjectDraw &subjects, std::string &error);

  [[nodiscard]] std::expected<uint32_t, std::string>
  RegisterPieceMaterials(SubjectDraw &subjects, SubjectDraw *glass, Geometry source);
  [[nodiscard]] bool
  RestorePieceMaterials(SubjectDraw &subjects, SubjectDraw *glass, std::string &error);

  [[nodiscard]] size_t PieceSourceBytes() const noexcept;

  [[nodiscard]] size_t PieceSlots() const noexcept { return Pieces_.size(); }

  [[nodiscard]] size_t PieceSlotBytes() const noexcept {
    return Pieces_.capacity() * sizeof(Piece);
  }

  [[nodiscard]] std::expected<HeightPageHandle, std::string>
  PlaceHeightPage(SubjectDraw &subjects, std::span<const float> nodes);
  void ReleaseHeightPage(SubjectDraw &subjects, HeightPageHandle which);
  [[nodiscard]] PageId HeightPageResident(HeightPageHandle which) const noexcept;
  [[nodiscard]] bool RestoreHeightPages(SubjectDraw &subjects, std::string &error);
  [[nodiscard]] bool
  SetGroundGrid(SubjectDraw &subjects, std::span<const float> fractions, std::string &error);
  [[nodiscard]] bool SetTerrainTiles(SubjectDraw &subjects,
                                     std::span<const TerrainTile> real,
                                     std::span<const TerrainTile> virtual_,
                                     std::string &error);
  [[nodiscard]] bool RestoreTerrain(SubjectDraw &subjects, std::string &error);
  [[nodiscard]] size_t HeightPageSourceBytes() const noexcept;

  [[nodiscard]] size_t HeightPageSlots() const noexcept { return HeightPages_.size(); }

  [[nodiscard]] size_t HeightPageSlotBytes() const noexcept {
    return HeightPages_.capacity() * sizeof(HeightPage);
  }

private:
  struct Piece {
    ResourceSlotState State{};
    std::vector<float> Tangents;
    std::vector<StoredVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<DagCluster> Clusters;
    std::vector<float> Colours;
    Mat4 Row;
    std::vector<Mat4> Rows;
    uint32_t MaxInstances = 0;
    PieceSurface Surface;
    PieceId Resident = kNoPiece;
    bool Textured = false;

    [[nodiscard]] PieceMesh Mesh() const noexcept;
  };

  [[nodiscard]] bool HasPiece(PieceHandle handle) const noexcept;

  struct PieceMaterials {
    Geometry Source;
    std::vector<SubjectMaterial> Slots;
  };

  [[nodiscard]] static std::expected<PieceMaterials, std::string>
  ResolvePieceMaterials(Geometry source);
  [[nodiscard]] bool AppendPieceMaterials(SubjectDraw &subjects,
                                          SubjectDraw *glass,
                                          const PieceMaterials &materials,
                                          std::string &error);

  struct HeightPage {
    ResourceSlotState State{};
    std::vector<float> Nodes;
    PageId Resident = kNoPage;
  };

  [[nodiscard]] bool HasHeightPage(HeightPageHandle handle) const noexcept;

  std::vector<Piece> Pieces_;
  uint32_t FirstFreePiece_ = kNoResourceSlot;
  std::vector<PieceMaterials> PieceMaterials_;
  std::vector<uint32_t> RegisteredPieceSlots_;
  std::vector<HeightPage> HeightPages_;
  uint32_t FirstFreeHeightPage_ = kNoResourceSlot;
  std::vector<float> GroundGrid_;
  std::vector<TerrainTile> GroundReal_;
  std::vector<TerrainTile> GroundVirtual_;
};

}
#endif
