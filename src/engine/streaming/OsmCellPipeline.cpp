#include "OsmSourceLoaderState.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
#include "OsmCellAcquisition.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kWaitingChunks = 2;
constexpr double kIoAwaitMs = 5.0;
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
  pipeline.Handle = Io_.Post([shared = pipeline.Shared,
                              access = Access_,
                              provider = Requested_.front(),
                              registry = Access_->Registry,
                              root = Root_,
                              revision = Revision_,
                              token] {
    const auto fail = [&shared](std::string error) {
      const std::scoped_lock lock(shared->Mutex);
      shared->Error = std::move(error);
    };
    if (!access->Wire) {
      fail("official original OSM needs a source transport");
      return;
    }
    if (!access->Store) {
      access->Store = std::make_unique<Data::ContentStore>(
          Data::ContentStore::Config{.Directory = access->Directory, .UtcSeconds = {}});
    }
    Data::OsmCellAcquisition reader(provider, *access->Store, *access->Wire, registry, root);
    while (!token.stop_requested()) {
      const auto now = access->Wire->NowMs();
      const auto deadline = access->CurrentDeadline(revision);
      if (!std::isfinite(now) || !std::isfinite(deadline) || now >= deadline) {
        fail("original OSM source acquisition deadline exceeded");
        return;
      }
      while (reader.PendingCount() < Data::OsmCellAcquisition::MaximumPendingCells) {
        std::optional<Data::GeoCellId> cell;
        {
          const std::scoped_lock lock(shared->Mutex);
          if (!shared->Requests.empty()) {
            cell = shared->Requests.front();
            shared->Requests.pop_front();
          }
        }
        if (!cell) { break; }
        if (auto started = reader.Start(*cell); !started) {
          fail(std::move(started.error()));
          return;
        }
      }
      auto ready = reader.TakeReady();
      if (!ready) {
        fail(std::move(ready.error()));
        return;
      }
      if (*ready) {
        std::unique_lock lock(shared->Mutex);
        while (shared->Ready.size() >= kWaitingChunks && !token.stop_requested()) {
          shared->Changed.wait_for(lock, std::chrono::milliseconds(5));
          const auto current = access->Wire->NowMs();
          const auto until = access->CurrentDeadline(revision);
          if (!std::isfinite(current) || !std::isfinite(until) || current >= until) {
            shared->Error = "original OSM source acquisition deadline exceeded";
            return;
          }
        }
        if (token.stop_requested()) { return; }
        shared->Ready.push_back(std::move(**ready));
        continue;
      }
      if (reader.PendingCount() != 0) {
        (void)access->Wire->Await(
            std::min(kIoAwaitMs, std::max(0.0, deadline - access->Wire->NowMs())));
      } else {
        std::unique_lock lock(shared->Mutex);
        shared->Changed.wait_for(lock, std::chrono::milliseconds(5), [&] {
          return !shared->Requests.empty() || token.stop_requested();
        });
      }
    }
  });
}

void OsmSourceLoader::PumpCellPipeline() {
  auto &pipeline = *CellPipeline_;
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
  if (!Pending_) {
    std::optional<Data::OsmSourceRead> ready;
    {
      const std::scoped_lock lock(pipeline.Shared->Mutex);
      if (!pipeline.Shared->Ready.empty()) {
        ready = std::move(pipeline.Shared->Ready.front());
        pipeline.Shared->Ready.pop_front();
      }
    }
    pipeline.Shared->Changed.notify_all();
    if (ready) {
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
  }
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
