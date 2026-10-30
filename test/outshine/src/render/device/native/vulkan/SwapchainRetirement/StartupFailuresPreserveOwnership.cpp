#include "src/render/device/native/vulkan/Presenter.h"
#include "Check.h"
#include <cerrno>
#include <cstddef>
#include <memory>
#include <pthread.h>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Render::Native::Vulkan;
size_t creates = 0;
size_t reject = 0;

void RetirementStartupFailure() {
  auto state = std::make_shared<PresentState>();
  auto target = std::make_shared<SwapchainState>();
  reject = creates + 1;
  const auto failed = state->Retirement.Start();
  CHECK(!failed && failed.error().Kind == ErrorKind::Driver,
        "thread startup failure returns the actual handled driver error");
  CHECK(!state->Retirement.Take(target) && target,
        "an unstarted retirement worker cannot consume a target");
  const auto before = creates;
  const auto started = state->Retirement.Start();
  CHECK(started, "a handled startup failure permits a fresh start");
  if (!started) { return; }
  CHECK(state->Retirement.Start() && creates == before + 1,
        "repeated successful start creates exactly one retirement thread");
  state->Retirement.Stop();
  CHECK(!state->Retirement.Take(target) && target,
        "stopped retirement preserves a subsequently refused target");
}

void PresentationStartupFailure() {
  Presenter presenter(std::make_shared<Context>());
  reject = creates + 2;
  const auto failed = presenter.Start();
  CHECK(!failed && failed.error().Kind == ErrorKind::Driver,
        "presentation thread failure propagates after retirement startup");
  const auto before = creates;
  const auto restarted = presenter.Start();
  CHECK(restarted && creates == before + 2,
        "failed presentation startup joins retirement and restarts both workers once");
  presenter.Stop();
}
}

extern "C" int RealCreateThread(pthread_t *,
                                const pthread_attr_t *,
                                void *(*)(void *),
                                void *) asm("__real_pthread_create");

extern "C" int WrappedCreateThread(pthread_t *thread,
                                   const pthread_attr_t *attributes,
                                   void *(*entry)(void *),
                                   void *argument) asm("__wrap_pthread_create");

extern "C" int WrappedCreateThread(pthread_t *thread,
                                   const pthread_attr_t *attributes,
                                   void *(*entry)(void *),
                                   void *argument) {
  ++creates;
  if (creates == reject) { return EAGAIN; }
  return RealCreateThread(thread, attributes, entry, argument);
}

int main() {
  RetirementStartupFailure();
  PresentationStartupFailure();
  outshine::Test::Covers("actual retirement and presentation pthread startup failures, refusal "
                         "ownership, retry, idempotence and joined shutdown without GPU handles");
  return outshine::Test::Report();
}
