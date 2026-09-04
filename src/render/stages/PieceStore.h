#ifndef OUTSHINE_RENDER_STAGES_PIECESTORE_H
#define OUTSHINE_RENDER_STAGES_PIECESTORE_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "math/Mat4.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "DrawList.h"
#include "SubjectResidency.h"

namespace outshine::Render {

struct PieceMesh {
  std::span<const StoredVertex> Verts;
  std::span<const uint32_t> Indices;
  std::span<const DagCluster> Clusters;
  std::span<const float> Colours;
  Mat4 Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  uint32_t MaterialSlot = 0;
  bool Textured = false;
};

using PieceId = uint32_t;
inline constexpr PieceId kNoPiece = ~0u;

class PieceStore {
public:
  void StandsOn(SDL_GPUDevice *device, bool filtersFloat32) {
    Res_.StandsOn(device, filtersFloat32);
  }

  [[nodiscard]] PieceId Place(const PieceMesh &piece, std::string &error);
  void Release(PieceId which);

  [[nodiscard]] bool Hand(std::string &error);
  [[nodiscard]] bool HandDrawArguments(bool deferred, std::string &error);

  void FlushCrossings(SDL_GPUCommandBuffer *commands) { Res_.FlushCrossings(commands); }

  [[nodiscard]] bool Empty() const { return Live_ == 0; }

  [[nodiscard]] uint32_t Pieces() const { return Live_; }

  [[nodiscard]] const SubjectResidency &Resident() const { return Res_; }

  [[nodiscard]] SubjectResidency &Resident() { return Res_; }

  [[nodiscard]] const std::vector<DrawBatch> &Drawn() const { return Batches_; }

  [[nodiscard]] const std::vector<VertexLayout> &Layouts() const { return Layouts_; }

  [[nodiscard]] uint32_t ClusterJobs() const { return Jobs_; }

  [[nodiscard]] uint32_t ClusterBatchRows() const {
    return Args_.empty() ? 0u : static_cast<uint32_t>(Batches_.size());
  }

  [[nodiscard]] bool Cut() const { return !Args_.empty(); }

  [[nodiscard]] uint32_t VerticesHeld() const { return TopV_; }

  [[nodiscard]] uint32_t IndicesHeld() const { return TopI_; }

  [[nodiscard]] uint32_t TrianglesLive() const { return TrianglesLive_; }

private:
  struct Range {
    uint32_t First = 0;
    uint32_t Count = 0;
  };

  struct Piece {
    Range V;
    Range I;
    uint32_t MaterialSlot = 0;
    VertexLayout Layout = VertexLayout::PositionNormal;
    Mat4 Row;
    std::vector<DagCluster> Clusters;
    bool Live = false;
  };

  [[nodiscard]] static Range Take(std::vector<Range> &free, uint32_t count, uint32_t &top);
  static void Give(std::vector<Range> &free, Range back);
  [[nodiscard]] bool Room(std::string &error);
  void Retable();

  SubjectResidency Res_;
  std::vector<Piece> Pieces_;
  std::vector<uint32_t> Spare_;
  std::vector<Range> FreeV_;
  std::vector<Range> FreeI_;
  uint32_t TopV_ = 0;
  uint32_t TopI_ = 0;
  uint32_t Live_ = 0;
  uint32_t TrianglesLive_ = 0;
  bool Dirty_ = false;

  std::vector<DrawBatch> Batches_;
  std::vector<VertexLayout> Layouts_;
  std::vector<float> Rows_;
  std::vector<float> Spheres_;
  std::vector<uint32_t> JobWords_;
  std::vector<uint32_t> BatchRows_;
  std::vector<uint32_t> Args_;
  uint32_t Jobs_ = 0;
};

} // namespace outshine::Render
#endif
