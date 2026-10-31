#ifndef OUTSHINE_TEST_NATIVEBASISLOOKUPFIXTURE_H
#define OUTSHINE_TEST_NATIVEBASISLOOKUPFIXTURE_H
#include "test/harness/engine/NativeBuildingSources.h"
#include <chrono>
#include <memory>
#include <semaphore>
#include <utility>

namespace NativeBasisFixture {
using namespace outshine;
using namespace outshine::Test;
using outshine::Test::BuildingSources::Fixture;
constexpr auto kDeadline = std::chrono::seconds(5);

inline bool Open(Fixture &env) {
  CHECK(env.Open(), "actual source, transport and native asset cache open");
  if (!env.Stack.BuildingAssets()) { return false; }
  const auto key = env.Generate();
  CHECK(key, "the actual native generator seeds a prepared building basis");
  return key.has_value();
}

inline auto Query(StructureBuildQueue &queue, Fixture &env) {
  return queue.PollNativeBasis(env.Stack, env.Stack.Footprints(), 0);
}

inline void Barrier(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the worker accepts a terminal barrier");
  if (barrier != Tasks::kNoTask) { pool.Wait(barrier); }
}

class WorkerBlock {
public:
  WorkerBlock() = default;

  ~WorkerBlock() { Release(); }

  WorkerBlock(const WorkerBlock &) = delete;
  WorkerBlock &operator=(const WorkerBlock &) = delete;

  bool Start(Tasks &pool) {
    State_ = std::make_shared<State>();
    if (!pool.PostDetached([state = State_] {
          state->Entered.release();
          state->Release.acquire();
        })) {
      State_.reset();
      return false;
    }
    return State_->Entered.try_acquire_for(kDeadline);
  }

  void Release() {
    if (const auto state = std::exchange(State_, {})) { state->Release.release(); }
  }

private:
  struct State {
    std::binary_semaphore Entered{0}, Release{0};
  };

  std::shared_ptr<State> State_;
};
}
#endif
