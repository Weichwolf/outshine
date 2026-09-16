#ifndef OUTSHINE_ENGINE_STRUCTUREBAKES_H
#define OUTSHINE_ENGINE_STRUCTUREBAKES_H

#include <expected>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "math/Vec3.h"

#include "HeightField.h"
#include "GroundStack.h"
#include "StructureBake.h"
#include "StructureMesher.h"
#include "Tasks.h"
#include "TilePieces.h"

namespace outshine {

class StructureBakes {
public:
  ~StructureBakes();

  void Opens(Tasks *pool, const StructureMesher *mesher) {
    Pool_ = pool;
    Mesher_ = mesher;
  }

  struct BakeRevision {
    uint64_t Vectors = 0;
    uint64_t Footprints = 0;
    double FocalPx = 0.0;
    double TileSpanM = 0.0;

    [[nodiscard]] bool Matches(const Ground::OsmField &vectors,
                               const Ground::BuildingField &footprints) const noexcept {
      return Vectors == vectors.Generation() && Footprints == footprints.Revision() &&
             FocalPx == footprints.FocalPx() && TileSpanM == footprints.TileSpanM();
    }
  };

  [[nodiscard]] size_t Posts(Ground::GroundStack &stack);

  struct Landing {
    uint32_t Tile = 0;
    const Generators::BakedTile *Baked = nullptr;
    Vec3 AnchorEcef;
    std::optional<Ground::BuildingField::PendingAcceptance> Footprints;
  };

  [[nodiscard]] std::expected<std::vector<Landing>, Generators::StructureBakeError>
  NextLandings(Ground::GroundStack &stack, size_t most);
  void CommitsLandings(Ground::GroundStack &stack, std::span<Landing> landings) noexcept;
  void Clear();

  [[nodiscard]] size_t Queued() const { return Queue_.size(); }

  [[nodiscard]] size_t Posted() const { return Posted_; }

  [[nodiscard]] size_t Landed() const { return Landed_; }

  [[nodiscard]] size_t Deferred() const { return Deferred_; }

  [[nodiscard]] size_t Discarded() const { return Discarded_; }

private:
  struct Output {
    Generators::BakedTile Tile;
    std::expected<void, Generators::StructureBakeError> Status;
  };

  struct Job {
    uint32_t Tile = 0;
    BakeRevision Revision;
    std::unique_ptr<Generators::RawTile> Raw;
    std::shared_ptr<const Ground::HeightField> Heights;
    std::unique_ptr<Output> Out;
    std::unique_ptr<MeshScratch> Scratch;
    Tasks::Handle Handle = Tasks::kNoTask;
  };

  template <typename T>
  [[nodiscard]] static std::unique_ptr<T> Borrowed(std::vector<std::unique_ptr<T>> &idle) {
    if (idle.empty()) { return std::make_unique<T>(); }
    std::unique_ptr<T> one = std::move(idle.back());
    idle.pop_back();
    return one;
  }

  [[nodiscard]] std::unique_ptr<MeshScratch> LentScratch();

  Tasks *Pool_ = nullptr;
  const StructureMesher *Mesher_ = nullptr;
  std::deque<Job> Queue_;
  std::vector<std::unique_ptr<Generators::RawTile>> IdleRaw_;
  std::vector<std::unique_ptr<Output>> IdleOut_;
  std::vector<std::unique_ptr<MeshScratch>> IdleScratch_;
  size_t Posted_ = 0;
  size_t Landed_ = 0;
  size_t Deferred_ = 0;
  size_t Discarded_ = 0;
};

}
#endif
