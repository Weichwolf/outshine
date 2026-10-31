#include "AcquisitionFixture.h"
#include "Check.h"
#include <chrono>
#include <atomic>
#include <filesystem>
#include <semaphore>
#include <thread>

namespace {
using namespace SourceAcquisitionFixture;

void Completion(const std::filesystem::path &directory, bool cells) {
  Gate gate;
  ProbeTransport wire(gate);
  Tasks compute(1);
  Tasks io(1);
  SourceAcquisition source({.Compute = compute, .Io = io}, &wire, directory.string());
  const auto unrelated = io.Post([] {});
  CHECK(io.AwaitCompletion(unrelated, 1), "an unrelated tracked completion remains retained");
  WorkerBlock held;
  CHECK(held.Start(io), "IO is held before the source job starts");
  CHECK(Start(source, cells), "region or cell demand starts without caller IO");
  CHECK(!source.AwaitSlice(0) && !source.AwaitSlice(0.002),
        "unrelated tracked completion cannot satisfy the source wait");
  std::binary_semaphore entered(0);
  std::binary_semaphore finished(0);
  std::atomic_bool progress{false};
  std::thread waiter([&] {
    entered.release();
    progress = source.AwaitSlice(1);
    finished.release();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the caller starts waiting before source IO can run");
  CHECK(!finished.try_acquire_for(std::chrono::milliseconds(1)),
        "the source waiter cannot wake from the unrelated completion");
  held.Release();
  waiter.join();
  CHECK(progress, "a delivered result wakes the source waiter without polling unrelated handles");
  Barrier(io);
  CHECK(source.AwaitSlice(1), "own completion/ready data remains observable before polling");
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    source.Poll();
    if (source.CurrentPhase() != SourceAcquisition::Phase::Loading && source.PendingCount() == 0) {
      break;
    }
    (void)source.AwaitSlice(0.005);
  } while (std::chrono::steady_clock::now() < until);
  CHECK(source.CurrentPhase() == SourceAcquisition::Phase::Ready && !gate.CallerIo,
        "the exact native source reaches Ready after IO/decode completion");
  CHECK(io.TakeCompletion(unrelated), "unrelated completion still belongs to its original owner");
}
}

int main() {
  const std::filesystem::path directory = outshine::Test::PlantedPath("source-completion");
  Completion(directory / "region", false);
  Completion(directory / "cells", true);
  std::filesystem::remove_all(directory);
  return outshine::Test::Report();
}
