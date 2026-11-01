#include "RetirementFixture.h"
#include "Check.h"
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

namespace {
struct DiscardSignal {
  std::binary_semaphore &Signal;

  ~DiscardSignal() { Signal.release(); }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Test::Retirement;
  auto pool = std::make_unique<Tasks>(1);
  Tasks *const closing = pool.get();
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  std::binary_semaphore discarded{0};
  CHECK(pool->PostDetached([&] {
    entered.release();
    (void)release.try_acquire_for(5s);
  }),
        "the pool accepts a worker that keeps its shutdown body alive");
  CHECK(entered.try_acquire_for(2s), "the real worker is blocked before shutdown");
  auto signal = std::make_shared<DiscardSignal>(discarded);
  CHECK(pool->PostDetached([signal] {}), "the shutdown discard observer is queued");
  signal.reset();
  const JobAdmission admission(1);
  const auto control = std::make_shared<Control>();
  control->Frame = std::this_thread::get_id();
  auto task = Task(admission, control);
  const auto *raw = &task.Raw();
  const auto *heights = &task.Heights();
  std::thread shutdown([owned = std::move(pool)] mutable { owned.reset(); });
  CHECK(discarded.try_acquire_for(2s), "pool shutdown has stopped admission and discarded work");
  CHECK(!task.Retire(*closing) && &task.Raw() == raw && &task.Heights() == heights &&
            !control->Freed.load() && admission.Active() == 1,
        "a rejected cleanup post restores its stopped payload and original credit");
  release.release();
  shutdown.join();
  Tasks cleanup(1);
  CHECK(task.Retire(cleanup), "the retained payload can retire through another live executor");
  Drain(cleanup);
  Released(admission, *control);
  return Report();
}
