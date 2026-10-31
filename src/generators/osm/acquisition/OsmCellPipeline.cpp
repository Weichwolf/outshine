#include "OsmSourceAcquisitionState.h"
#include "OsmCellAcquisition.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <Units.h>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kWaitingChunks = 2;
}

void SourceAcquisition::CancelCellPipeline() {
  if (!CellPipeline_) { return; }
  (void)CellPipeline_->Stop.request_stop();
  CellPipeline_->Shared->Progress->Changed.notify_all();
}

void SourceAcquisition::StartCellPipeline() {
  CellPipeline_ = std::make_shared<CellPipeline>();
  CellPipeline_->Context = Access_;
  CellPipeline_->Shared->Progress = Access_->Progress;
  auto &pipeline = *CellPipeline_;
  pipeline.Revision = Revision_;
  const auto token = pipeline.Stop.get_token();
  const auto complete =
      std::make_shared<AcquisitionCompletion<CellPipeline::Exchange>>(pipeline.Shared);
  const bool accepted = Io_.PostDetachedSteps([work = CellPipeline_,
                                               complete,
                                               provider = Requested_.front(),
                                               registry = Access_->Registry,
                                               root = Root_,
                                               revision = Revision_,
                                               token] {
    (void)complete;
    return work->Acquire(provider, registry, root, revision, token);
  });
  if (!accepted) {
    const std::scoped_lock lock(pipeline.Shared->Progress->Mutex);
    pipeline.Shared->Error = "original OSM IO queue is closed";
  }
}

Tasks::StepResult SourceAcquisition::CellPipeline::Fail(std::string error) {
  Reader.reset();
  const std::scoped_lock lock(Shared->Progress->Mutex);
  Shared->Error = std::move(error);
  return Tasks::StepResult::Complete;
}

bool SourceAcquisition::CellPipeline::DeadlineExceeded(Access &access, uint64_t revision) {
  const auto now = access.Wire->NowMs();
  const auto deadline = access.CurrentDeadline(revision);
  return !std::isfinite(now) || !std::isfinite(deadline) || now >= deadline;
}

std::expected<void, std::string>
SourceAcquisition::CellPipeline::StartQueued(CellAcquisition &reader,
                                             const std::stop_token &stop) const {
  while (!stop.stop_requested() && reader.PendingCount() < CellAcquisition::MaximumPendingCells) {
    std::optional<GeoCellId> cell;
    {
      const std::scoped_lock lock(Shared->Progress->Mutex);
      if (!Shared->Requests.empty()) {
        cell = Shared->Requests.front();
        Shared->Requests.pop_front();
      }
    }
    if (!cell) { break; }
    if (auto started = reader.Start(*cell); !started) {
      return std::unexpected(std::move(started.error()));
    }
  }
  return {};
}

void SourceAcquisition::CellPipeline::Deliver(SourceRead ready) const {
  {
    const std::scoped_lock lock(Shared->Progress->Mutex);
    Shared->Ready.push_back(std::move(ready));
  }
  Shared->Progress->Changed.notify_all();
}

Tasks::StepResult SourceAcquisition::CellPipeline::Acquire(const Data::SourceProvider &provider,
                                                           const Data::ProviderRegistry *registry,
                                                           std::string_view root,
                                                           uint64_t revision,
                                                           const std::stop_token &stop) {
  const auto &access = Context;
  if (stop.stop_requested()) {
    Reader.reset();
    return Tasks::StepResult::Complete;
  }
  if (access->Wire == nullptr) { return Fail("official original OSM needs a source transport"); }
  if (DeadlineExceeded(*access, revision)) {
    return Fail("original OSM source acquisition deadline exceeded");
  }
  if (!access->Store) {
    access->Store = std::make_unique<Data::RawSourceCache>(
        Data::RawSourceCache::Config{.Directory = access->Directory});
  }
  if (!Reader) {
    Reader = std::make_shared<CellAcquisition>(
        provider, *access->Store, *access->Wire, registry, std::string(root));
  }
  {
    std::unique_lock lock(Shared->Progress->Mutex);
    if (Shared->Ready.size() >= kWaitingChunks) {
      Shared->Progress->Changed.wait_for(
          lock, std::chrono::duration<double>(MaximumIoAwaitSeconds), [&] {
            return Shared->Ready.size() < kWaitingChunks || stop.stop_requested();
          });
      return Tasks::StepResult::Yield;
    }
  }
  if (auto started = StartQueued(*Reader, stop); !started) {
    return Fail(std::move(started.error()));
  }
  for (size_t index = 0; index < kWaitingChunks; ++index) {
    if (stop.stop_requested()) {
      Reader.reset();
      return Tasks::StepResult::Complete;
    }
    {
      const std::scoped_lock lock(Shared->Progress->Mutex);
      if (Shared->Ready.size() >= kWaitingChunks) { return Tasks::StepResult::Yield; }
    }
    auto ready = Reader->TakeReady();
    if (!ready) { return Fail(std::move(ready.error())); }
    if (!*ready) { break; }
    Deliver(std::move(**ready));
  }
  if (Reader->PendingCount() != 0) {
    const auto remaining = access->CurrentDeadline(revision) - access->Wire->NowMs();
    (void)access->Wire->Await(std::min(MaximumIoAwaitSeconds * kMsPerS, std::max(0.0, remaining)));
  } else {
    std::unique_lock lock(Shared->Progress->Mutex);
    Shared->Progress->Changed.wait_for(
        lock, std::chrono::duration<double>(MaximumIoAwaitSeconds), [&] {
          return !Shared->Requests.empty() || stop.stop_requested();
        });
  }
  return Tasks::StepResult::Yield;
}

void SourceAcquisition::PumpCellPipeline() {
  const auto &pipeline = *CellPipeline_;
  if (pipeline.Revision != Revision_ || Phase_ != Phase::Loading) { CancelCellPipeline(); }
  if (pipeline.Shared->Finished.load(std::memory_order_acquire)) {
    if (pipeline.Revision == Revision_ && Phase_ == Phase::Loading) {
      const std::scoped_lock lock(pipeline.Shared->Progress->Mutex);
      Error_ = pipeline.Shared->Error.empty()
                   ? "original OSM acquisition stopped before coverage was complete"
                   : pipeline.Shared->Error;
      Cells_->Preparing.clear();
      Phase_ = Phase::Failed;
      if (Pending_) { (void)Pending_->Stop.request_stop(); }
    }
    CellPipeline_.reset();
    return;
  }
  if (pipeline.Revision != Revision_ || Phase_ != Phase::Loading) { return; }
  if (!Pending_) { ConsumeAcquiredCell(); }
  if (Phase_ == Phase::Loading) { QueueMissingCells(); }
}

void SourceAcquisition::ConsumeAcquiredCell() {
  auto &pipeline = *CellPipeline_;
  std::optional<SourceRead> ready;
  {
    const std::scoped_lock lock(pipeline.Shared->Progress->Mutex);
    if (!pipeline.Shared->Ready.empty()) {
      ready = std::move(pipeline.Shared->Ready.front());
      pipeline.Shared->Ready.pop_front();
    }
  }
  pipeline.Shared->Progress->Changed.notify_all();
  if (!ready) { return; }
  if (auto refined = Cells_->Refine(ready->Refine); !refined) {
    Error_ = std::move(refined.error());
    Cells_->Preparing.clear();
    Phase_ = Phase::Failed;
    CancelCellPipeline();
    return;
  }
  for (const auto cell : ready->Refine) { std::erase(pipeline.Assigned, cell); }
  if (!ready->Chunks.empty()) {
    StartDecode(std::move(ready->Chunks), pipeline.Stop, ready->ElapsedMs);
  }
}

void SourceAcquisition::ReleaseAssignedCells(std::span<const CellSource> ready) {
  if (!CellPipeline_ || CellPipeline_->Revision != Revision_) { return; }
  for (const auto &entry : ready) { std::erase(CellPipeline_->Assigned, entry.Address); }
}

void SourceAcquisition::QueueMissingCells() {
  auto &pipeline = *CellPipeline_;
  while (pipeline.Assigned.size() < CellAcquisition::MaximumPendingCells) {
    auto cells = Cells_->NextAcquisitionBatch(pipeline.Assigned);
    if (cells.empty()) { break; }
    const auto count =
        std::min(cells.size(), CellAcquisition::MaximumPendingCells - pipeline.Assigned.size());
    cells.resize(count);
    {
      const std::scoped_lock lock(pipeline.Shared->Progress->Mutex);
      pipeline.Shared->Requests.insert(pipeline.Shared->Requests.end(), cells.begin(), cells.end());
    }
    pipeline.Assigned.insert(pipeline.Assigned.end(), cells.begin(), cells.end());
    pipeline.Shared->Progress->Changed.notify_all();
  }
}
}
