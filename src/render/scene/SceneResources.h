#ifndef OUTSHINE_RENDER_SCENE_SCENERESOURCES_H
#define OUTSHINE_RENDER_SCENE_SCENERESOURCES_H

#include "ResourceHandle.h"
#include "SubjectTypes.h"

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

  void CopyPieceSourcesFrom(const SceneResources &source);
  [[nodiscard]] bool RestorePieces(SubjectDraw &subjects, std::string &error);

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

  struct HeightPage {
    ResourceSlotState State{};
    std::vector<float> Nodes;
    PageId Resident = kNoPage;
  };

  [[nodiscard]] bool HasHeightPage(HeightPageHandle handle) const noexcept;

  std::vector<Piece> Pieces_;
  uint32_t FirstFreePiece_ = kNoResourceSlot;
  std::vector<HeightPage> HeightPages_;
  uint32_t FirstFreeHeightPage_ = kNoResourceSlot;
};

}
#endif
