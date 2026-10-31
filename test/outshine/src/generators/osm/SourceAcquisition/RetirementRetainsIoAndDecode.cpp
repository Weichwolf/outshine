#include "AcquisitionFixture.h"
#include "Check.h"
#include <chrono>
#include <atomic>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <thread>

namespace {
using namespace SourceAcquisitionFixture;

void Running(const std::filesystem::path &directory, Hold stage, bool cells) {
  Gate gate;
  gate.Stage = stage;
  ProbeTransport wire(gate);
  Tasks compute(1);
  Tasks io(1);
  auto compiler = std::make_shared<Compiler>(gate);
  const std::weak_ptr<const CellCompiler> compilerLife = compiler;
  auto source =
      std::make_unique<SourceAcquisition>(SourceAcquisition::Workers{.Compute = compute, .Io = io},
                                          &wire,
                                          directory.string(),
                                          compiler);
  compiler.reset();
  CHECK(Start(*source, cells), "original source work is admitted");
  bool entered = false;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (!entered && std::chrono::steady_clock::now() < until) {
    source->Poll();
    entered = gate.Entered.try_acquire_for(std::chrono::milliseconds(1));
  }
  CHECK(entered, "the real acquisition or cell compiler is held inside its callback");
  std::binary_semaphore retired(0);
  std::thread release([&] {
    source.reset();
    retired.release();
  });
  CHECK(retired.try_acquire_for(std::chrono::milliseconds(250)),
        "source retirement returns before the held callback is released");
  CHECK(!compilerLife.expired(), "retired work and cleanup retain their compiler");
  if (stage == Hold::Compiler) {
    CHECK(!gate.Input.expired(), "a running compiler retains its exact decoded source snapshot");
  }
  gate.Release.release();
  release.join();
  const auto finish = std::chrono::steady_clock::now() + kDeadline;
  do {
    Barrier(compute);
    Barrier(io);
  } while (!compilerLife.expired() && std::chrono::steady_clock::now() < finish);
  CHECK(compilerLife.expired() && gate.CompilerOffOwner && !gate.CallerIo,
        "all retired owners are released on workers without caller IO");
  if (stage == Hold::Io) {
    CHECK(gate.Polls == 1 && gate.Canceled == 1 && gate.Compiled == 0,
          "stopped region/cell IO cancels exactly once without starting a decoder");
  } else {
    CHECK(gate.Compiled == 1 && gate.Input.expired(),
          "the only running decoder releases its input after cancellation");
  }
}

void Discard(const std::filesystem::path &directory, bool request, bool cells) {
  Gate gate;
  ProbeTransport wire(gate);
  Tasks compute(1);
  auto io = std::make_unique<Tasks>(1);
  WorkerBlock held;
  CHECK(held.Start(*io), "the IO pool is held before queued work or cleanup");
  auto compiler = std::make_shared<Compiler>(gate);
  const std::weak_ptr<const CellCompiler> compilerLife = compiler;
  auto source =
      std::make_unique<SourceAcquisition>(SourceAcquisition::Workers{.Compute = compute, .Io = *io},
                                          &wire,
                                          directory.string(),
                                          compiler);
  compiler.reset();
  if (request) { CHECK(Start(*source, cells), "the source request is queued behind the blocker"); }
  std::binary_semaphore retired(0);
  std::thread release([&] {
    source.reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "idle and queued source destruction return before pool shutdown");
  if (!returned) { held.Release(); }
  release.join();
  if (!returned) { return; }
  CHECK(!compilerLife.expired(), "idle resources stay queued rather than releasing on the caller");
  std::atomic_bool stopped{false};
  std::thread stop([&] {
    io.reset();
    stopped = true;
  });
  const bool disposed = gate.CompilerGone.try_acquire_for(kDeadline);
  CHECK(disposed && compilerLife.expired() && gate.CompilerOffOwner && !stopped,
        "shutdown discards retained inputs before joining the held worker");
  held.Release();
  stop.join();
  CHECK(stopped && gate.Begins == 0 && gate.Polls == 0 && gate.Compiled == 0,
        "discarded requests perform no acquisition or decode");
}
}

int main() {
  const std::filesystem::path directory = outshine::Test::PlantedPath("source-retirement");
  Running(directory / "region", Hold::Io, false);
  Running(directory / "cells", Hold::Io, true);
  Running(directory / "decode", Hold::Compiler, true);
  Discard(directory / "idle", false, false);
  Discard(directory / "queued-region", true, false);
  Discard(directory / "queued-cells", true, true);
  std::filesystem::remove_all(directory);
  return outshine::Test::Report();
}
