#include "ClassificationBuild.h"
#include "Capacity.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace outshine::Ground {
namespace {
size_t GridBytes(const ClassStructure::Grid &grid) {
  return CapacityBytes(grid.Cells) + CapacityBytes(grid.Seeds) + CapacityBytes(grid.Refs) +
         CapacityBytes(grid.Edges);
}
}

struct ClassificationBuild::Work {
  enum class Stage { Idle, Building, Done };
  std::mutex Mutex;
  std::condition_variable Completion;
  Stage Status = Stage::Idle;
  std::optional<Job> Pending;
  std::optional<Handback> Result;
  Generators::ClassificationRasterizer Rasterizer;
  std::shared_ptr<const ClassStructure::Grid> Fine = std::make_shared<const ClassStructure::Grid>();
  std::shared_ptr<const ClassStructure::Grid> Coarse =
      std::make_shared<const ClassStructure::Grid>();
  SourceRevision FineSource;
  SourceRevision CoarseSource;
  uint64_t Version = 0;
  std::atomic<size_t> HeapBytes{0};
  std::atomic<size_t> InputBytes{0};
  std::atomic<size_t> UploadBytes{0};
};

ClassificationBuild::ClassificationBuild(Tasks &pool)
    : Pool_(pool), Work_(std::make_shared<Work>()) {}

ClassificationBuild::~ClassificationBuild() {
  Cancel();
}

void ClassificationBuild::Cancel() noexcept {
  Stop_.request_stop();
}

bool ClassificationBuild::Submit(Job job) {
  Stop_ = std::stop_source{};
  {
    const std::scoped_lock lock(Work_->Mutex);
    assert(Work_->Status == Work::Stage::Idle);
    Work_->InputBytes.store(CapacityBytes(job.Raster.Points) + CapacityBytes(job.Raster.Rings) +
                                CapacityBytes(job.Raster.Features),
                            std::memory_order_relaxed);
    Work_->Pending = std::move(job);
    Work_->Status = Work::Stage::Building;
  }
  if (Pool_.PostDetached([work = Work_, stop = Stop_.get_token()] { Run(*work, stop); })) {
    return true;
  }
  {
    const std::scoped_lock lock(Work_->Mutex);
    Work_->Result.emplace(
        Handback{.Structure = {}, .Upload = {}, .Returned = std::move(*Work_->Pending)});
    Work_->Pending.reset();
    Work_->Status = Work::Stage::Done;
  }
  Work_->Completion.notify_all();
  return false;
}

void ClassificationBuild::Run(Work &work, const std::stop_token &stop) {
  Job job;
  {
    const std::scoped_lock lock(work.Mutex);
    assert(work.Pending.has_value());
    job = std::move(*work.Pending);
    work.Pending.reset();
  }
  Handback result;
  if (const auto built = work.Rasterizer.Build(job.Raster, stop); built && !stop.stop_requested()) {
    if (work.FineSource.Frame != job.Source.Frame || work.FineSource.Fine != job.Source.Fine) {
      work.Fine = std::make_shared<const ClassStructure::Grid>();
    }
    if (work.CoarseSource.Frame != job.Source.Frame ||
        work.CoarseSource.Coarse != job.Source.Coarse) {
      work.Coarse = std::make_shared<const ClassStructure::Grid>();
    }
    if (job.Grain == ClassGrain::Fine) {
      work.Fine = built->Grid;
      work.FineSource = job.Source;
    } else {
      work.Coarse = built->Grid;
      work.CoarseSource = job.Source;
    }
    result.BuildMs = built->BuildMs;
    result.Structure = std::make_shared<const ClassStructure>(
        job.Frame,
        work.Fine,
        work.Coarse,
        ClassStructure::FromRun{.Version = ++work.Version,
                                .UnmappedRow = job.UnmappedRow,
                                .BuildMs = built->BuildMs,
                                .Overflow = built->Overflow});
    result.Upload = std::make_shared<const Render::GroundClassBuffer>(*result.Structure);
    work.UploadBytes.store(result.Upload->HeapBytes(), std::memory_order_relaxed);
    work.HeapBytes.store(GridBytes(*work.Fine) + GridBytes(*work.Coarse) +
                             work.Rasterizer.ScratchBytes(),
                         std::memory_order_relaxed);
  }
  result.Returned = std::move(job);
  {
    const std::scoped_lock lock(work.Mutex);
    work.Result = std::move(result);
    work.Status = Work::Stage::Done;
  }
  work.Completion.notify_all();
}

std::optional<ClassificationBuild::Handback> ClassificationBuild::Collect() {
  const std::scoped_lock lock(Work_->Mutex);
  if (Work_->Status != Work::Stage::Done) { return std::nullopt; }
  if (Stop_.stop_requested() && Work_->Result) {
    Work_->Result->Structure.reset();
    Work_->Result->Upload.reset();
  }
  Work_->Status = Work::Stage::Idle;
  Work_->InputBytes.store(0, std::memory_order_relaxed);
  Work_->UploadBytes.store(0, std::memory_order_relaxed);
  return std::exchange(Work_->Result, std::nullopt);
}

bool ClassificationBuild::AwaitCompletion(double seconds) {
  if (!(seconds > 0)) { return false; }
  std::unique_lock lock(Work_->Mutex);
  return Work_->Completion.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
    return Work_->Status != Work::Stage::Building;
  });
}

size_t ClassificationBuild::HeapBytes() const {
  return Work_->HeapBytes.load(std::memory_order_relaxed) +
         Work_->InputBytes.load(std::memory_order_relaxed) +
         Work_->UploadBytes.load(std::memory_order_relaxed);
}
}
