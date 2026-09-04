#ifndef OUTSHINE_ENGINE_STRUCTUREBAKES_H
#define OUTSHINE_ENGINE_STRUCTUREBAKES_H

#include <cstdint>
#include <deque>
#include <memory>

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
    std::shared_ptr<Generators::RawTile> Raw;
    std::shared_ptr<const Ground::HeightField> Heights;
    std::shared_ptr<Generators::BakedTile> Out;
    Tasks::Handle Handle = Tasks::kNoTask;
  };

  Tasks *Pool_ = nullptr;
  const StructureMesher *Mesher_ = nullptr;
  std::deque<Job> Queue_;
  size_t Posted_ = 0;
  size_t Landed_ = 0;
  size_t Deferred_ = 0;
};

} // namespace outshine
#endif
