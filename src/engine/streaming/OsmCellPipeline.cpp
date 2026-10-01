#include "OsmSourceLoaderState.h"
#include "OsmCellAcquisition.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <math/Units.h>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kWaitingChunks = 2;
}

void OsmSourceLoader::CancelCellPipeline() {
  if (!CellPipeline_) { return; }
  (void)CellPipeline_->Stop.request_stop();
  CellPipeline_->Shared->Changed.notify_all();
}

void OsmSourceLoader::StartCellPipeline() {
  CellPipeline_ = std::make_unique<CellPipeline>();
  auto &pipeline = *CellPipeline_;
  pipeline.Revision = Revision_;
  const auto token = pipeline.Stop.get_token();
  pipeline.Handle =
      Io_.Post([work = &pipeline,
                access = Access_,
                provider = Requested_.front(),
                registry = Access_->Registry,
                root = Root_,
                revision = Revision_,
                token] { work->Acquire(access, provider, registry, root, revision, token); });
}

void OsmSourceLoader::CellPipeline::Fail(std::string error) const {
  const std::scoped_lock lock(Shared->Mutex);
  Shared->Error = std::move(error);
}

bool OsmSourceLoader::CellPipeline::DeadlineExceeded(Access &access, uint64_t revision) {
  const auto now = access.Wire->NowMs();
  const auto deadline = access.CurrentDeadline(revision);
  return !std::isfinite(now) || !std::isfinite(deadline) || now >= deadline;
}

std::expected<void, std::string>
OsmSourceLoader::CellPipeline::StartQueued(Data::OsmCellAcquisition &reader) const {
  while (reader.PendingCount() < Data::OsmCellAcquisition::MaximumPendingCells) {
    std::optional<Data::GeoCellId> cell;
    {
      const std::scoped_lock lock(Shared->Mutex);
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

bool OsmSourceLoader::CellPipeline::Deliver(Data::OsmSourceRead ready,
                                            Access &access,
                                            uint64_t revision,
                                            const std::stop_token &stop) const {
  std::unique_lock lock(Shared->Mutex);
  while (Shared->Ready.size() >= kWaitingChunks && !stop.stop_requested()) {
    Shared->Changed.wait_for(lock, std::chrono::duration<double>(MaximumIoAwaitSeconds));
    if (DeadlineExceeded(access, revision)) {
      Shared->Error = "original OSM source acquisition deadline exceeded";
      return false;
    }
  }
  if (stop.stop_requested()) { return false; }
  Shared->Ready.push_back(std::move(ready));
  return true;
}

void OsmSourceLoader::CellPipeline::Acquire(const std::shared_ptr<Access> &access,
                                            Data::SourceProvider provider,
                                            const Data::ProviderRegistry *registry,
                                            std::string root,
                                            uint64_t revision,
                                            const std::stop_token &stop) const {
  if (access->Wire == nullptr) {
    Fail("official original OSM needs a source transport");
    return;
  }
  if (!access->Store) {
    access->Store = std::make_unique<Data::ContentStore>(
        Data::ContentStore::Config{.Directory = access->Directory, .UtcSeconds = {}});
  }
  Data::OsmCellAcquisition reader(
      std::move(provider), *access->Store, *access->Wire, registry, std::move(root));
  while (!stop.stop_requested()) {
    if (DeadlineExceeded(*access, revision)) {
      Fail("original OSM source acquisition deadline exceeded");
      return;
    }
    if (auto started = StartQueued(reader); !started) {
      Fail(std::move(started.error()));
      return;
    }
    auto ready = reader.TakeReady();
    if (!ready) {
      Fail(std::move(ready.error()));
      return;
    }
    if (*ready) {
      if (!Deliver(std::move(**ready), *access, revision, stop)) { return; }
      continue;
    }
    if (reader.PendingCount() != 0) {
      const auto remaining = access->CurrentDeadline(revision) - access->Wire->NowMs();
      (void)access->Wire->Await(
          std::min(MaximumIoAwaitSeconds * kMsPerS, std::max(0.0, remaining)));
    } else {
      std::unique_lock lock(Shared->Mutex);
      Shared->Changed.wait_for(lock, std::chrono::duration<double>(MaximumIoAwaitSeconds), [&] {
        return !Shared->Requests.empty() || stop.stop_requested();
      });
    }
  }
}

void OsmSourceLoader::PumpCellPipeline() {
  const auto &pipeline = *CellPipeline_;
  if (pipeline.Revision != Revision_ || Phase_ != Phase::Loading) { CancelCellPipeline(); }
  if (Io_.TakeCompletion(pipeline.Handle)) {
    if (pipeline.Revision == Revision_ && Phase_ == Phase::Loading) {
      const std::scoped_lock lock(pipeline.Shared->Mutex);
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

void OsmSourceLoader::ConsumeAcquiredCell() {
  auto &pipeline = *CellPipeline_;
  std::optional<Data::OsmSourceRead> ready;
  {
    const std::scoped_lock lock(pipeline.Shared->Mutex);
    if (!pipeline.Shared->Ready.empty()) {
      ready = std::move(pipeline.Shared->Ready.front());
      pipeline.Shared->Ready.pop_front();
    }
  }
  pipeline.Shared->Changed.notify_all();
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

void OsmSourceLoader::ReleaseAssignedCells(std::span<const CellSource> ready) {
  if (!CellPipeline_ || CellPipeline_->Revision != Revision_) { return; }
  for (const auto &entry : ready) {
    const auto cell = entry.Snapshot->Cell;
    if (cell) { std::erase(CellPipeline_->Assigned, *cell); }
  }
}

void OsmSourceLoader::QueueMissingCells() {
  auto &pipeline = *CellPipeline_;
  while (pipeline.Assigned.size() < Data::OsmCellAcquisition::MaximumPendingCells) {
    auto cells = Cells_->NextAcquisitionBatch(pipeline.Assigned);
    if (cells.empty()) { break; }
    const auto count = std::min(
        cells.size(), Data::OsmCellAcquisition::MaximumPendingCells - pipeline.Assigned.size());
    cells.resize(count);
    {
      const std::scoped_lock lock(pipeline.Shared->Mutex);
      pipeline.Shared->Requests.insert(pipeline.Shared->Requests.end(), cells.begin(), cells.end());
    }
    pipeline.Assigned.insert(pipeline.Assigned.end(), cells.begin(), cells.end());
    pipeline.Shared->Changed.notify_all();
  }
}
}
