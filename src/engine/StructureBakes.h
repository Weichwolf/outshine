#ifndef OUTSHINE_ENGINE_STRUCTUREBAKES_H
#define OUTSHINE_ENGINE_STRUCTUREBAKES_H

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "HeightField.h"
#include "GroundStack.h"
#include "StructureBake.h"
#include "StructureMesher.h"
#include "Tasks.h"
#include "TilePieces.h"

namespace outshine {

class StructureBakes {
public:
  void Opens(Tasks *pool, const StructureMesher *mesher) {
    Pool_ = pool;
    Mesher_ = mesher;
  }

  [[nodiscard]] size_t Posts(Ground::GroundStack &stack);
  [[nodiscard]] size_t Lands(Ground::GroundStack &stack, TilePieces &pieces, size_t most);
  void Clear();

  [[nodiscard]] size_t Queued() const { return Queue_.size(); }

  [[nodiscard]] size_t Posted() const { return Posted_; }

  [[nodiscard]] size_t Landed() const { return Landed_; }

  [[nodiscard]] size_t Deferred() const { return Deferred_; }

private:
  struct Job {
    uint32_t Tile = 0;
    std::unique_ptr<Generators::RawTile> Raw;
    std::shared_ptr<const Ground::HeightField> Heights;
    std::unique_ptr<Generators::BakedTile> Out;
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
  std::vector<std::unique_ptr<Generators::BakedTile>> IdleOut_;
  std::vector<std::unique_ptr<MeshScratch>> IdleScratch_;
  size_t Posted_ = 0;
  size_t Landed_ = 0;
  size_t Deferred_ = 0;
};

} // namespace outshine
#endif
