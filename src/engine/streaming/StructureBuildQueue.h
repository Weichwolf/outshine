#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTUREBUILDQUEUE_H

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
#include "StructureBuildTask.h"
#include "StructureMesher.h"
#include "Tasks.h"

namespace outshine {

class StructureBuildQueue {
public:
  ~StructureBuildQueue();

  void Opens(Tasks *pool, const StructureMesher *mesher) {
    Pool_ = pool;
    Mesher_ = mesher;
  }

  struct BakeRevision {
    uint64_t Vectors = 0;
    double FocalPx = 0.0;
    double TileSpanM = 0.0;
    LongitudeLatitude Eye;

    [[nodiscard]] bool Matches(const Ground::OsmField &vectors,
                               const Ground::BuildingField &footprints,
                               LongitudeLatitude eye) const noexcept {
      return Vectors == vectors.Generation() && FocalPx == footprints.FocalPx() &&
             TileSpanM == footprints.TileSpanM() && Eye.LongitudeDeg == eye.LongitudeDeg &&
             Eye.LatitudeDeg == eye.LatitudeDeg;
    }
  };

  [[nodiscard]] size_t
  Posts(Ground::GroundStack &stack, Ground::BuildingField &footprints, LongitudeLatitude eye);

  struct Landing {
    uint32_t Tile = 0;
    const Generators::BakedTile *Baked = nullptr;
    Vec3 AnchorEcef;
    std::optional<Ground::BuildingField::PendingAcceptance> Footprints;
  };

  [[nodiscard]] std::expected<std::vector<Landing>, Generators::StructureBakeError>
  NextLandings(Ground::GroundStack &stack,
               Ground::BuildingField &footprints,
               LongitudeLatitude eye,
               size_t most);
  void ResumeCompletedSlices();
  void CommitsLandings(Ground::GroundStack &stack,
                       Ground::BuildingField &footprints,
                       std::span<Landing> landings) noexcept;
  void Clear();

  [[nodiscard]] size_t Queued() const { return Queue_.size(); }

  [[nodiscard]] bool Complete(const Ground::GroundStack &stack,
                              const Ground::BuildingField &footprints) const;

  [[nodiscard]] size_t Posted() const { return Posted_; }

  [[nodiscard]] size_t Landed() const { return Landed_; }

  [[nodiscard]] size_t Deferred() const { return Deferred_; }

  [[nodiscard]] size_t Discarded() const { return Discarded_; }

  [[nodiscard]] double MeanBakeMs() const {
    return Landed_ > 0 ? BakedMs_ / static_cast<double>(Landed_) : 0.0;
  }

  [[nodiscard]] double SlowestBakeMs() const { return SlowestBakeMs_; }

  [[nodiscard]] double SlowestSliceMs() const { return SlowestSliceMs_; }

  [[nodiscard]] double SlowestTaskMs() const { return SlowestTaskMs_; }

  [[nodiscard]] size_t QueuedStructures() const;

  [[nodiscard]] bool AwaitSlice(double seconds) const {
    return Pool_ != nullptr && !Queue_.empty() && Pool_->AwaitCompletion(seconds);
  }

private:
  struct QueuedBuild {
    BakeRevision Revision;
    StructureBuildTask Task;
    size_t BakedStructures = 0;
    size_t Slices = 0;
    double SlowestSliceMs = 0.0;
    double SlowestTaskMs = 0.0;
    bool Finished = false;
  };

  template <typename T>
  [[nodiscard]] static std::unique_ptr<T> Borrowed(std::vector<std::unique_ptr<T>> &idle) {
    if (idle.empty()) { return std::make_unique<T>(); }
    std::unique_ptr<T> one = std::move(idle.back());
    idle.pop_back();
    return one;
  }

  [[nodiscard]] std::unique_ptr<MeshScratch> LentScratch();
  void PostSlice(QueuedBuild &build);
  void DiscardStale(const Ground::OsmField &vectors,
                    Ground::BuildingField &prints,
                    LongitudeLatitude eye);

  Tasks *Pool_ = nullptr;
  const StructureMesher *Mesher_ = nullptr;
  std::deque<QueuedBuild> Queue_;
  std::vector<std::unique_ptr<Generators::RawTile>> IdleRaw_;
  std::vector<std::unique_ptr<StructureBuildTask::Output>> IdleOut_;
  std::vector<std::unique_ptr<MeshScratch>> IdleScratch_;
  size_t Posted_ = 0;
  size_t Landed_ = 0;
  size_t Deferred_ = 0;
  size_t Discarded_ = 0;
  double BakedMs_ = 0.0;
  double SlowestBakeMs_ = 0.0;
  double SlowestSliceMs_ = 0.0;
  double SlowestTaskMs_ = 0.0;
};

}
#endif
