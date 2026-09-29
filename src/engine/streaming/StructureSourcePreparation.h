#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTURESOURCEPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTURESOURCEPREPARATION_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "HeightField.h"
#include "SourcedTerrainFields.h"
#include "Tasks.h"

namespace outshine {

class StructureSourcePreparation {
public:
  enum class State : uint8_t { Preparing, Ready, Failed, Cancelled, OverBudget };

  StructureSourcePreparation(Tasks &pool,
                             const Ground::HeightField::Request &request,
                             SourcedTerrainFields sources,
                             size_t bytesMost);
  ~StructureSourcePreparation();
  StructureSourcePreparation(const StructureSourcePreparation &) = delete;
  StructureSourcePreparation &operator=(const StructureSourcePreparation &) = delete;
  StructureSourcePreparation(StructureSourcePreparation &&other) noexcept;
  StructureSourcePreparation &operator=(StructureSourcePreparation &&other) noexcept;

  [[nodiscard]] State Advance();
  void Cancel() noexcept;

  [[nodiscard]] bool Running() const noexcept { return Handle_ != Tasks::kNoTask; }

  [[nodiscard]] std::shared_ptr<const Ground::HeightField> Result() const noexcept;

private:
  struct Work {
    std::atomic_bool Stopping = false;
    Ground::HeightField::Request Request;
    SourcedTerrainFields Sources;
    std::vector<Ground::HeightField::Block> Blocks;
    std::shared_ptr<const Ground::HeightField> Result;
    State Outcome = State::Preparing;
    size_t Next = 0;
  };

  void Join();

  Tasks *Pool_ = nullptr;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  std::unique_ptr<Work> Work_;
};

}
#endif
