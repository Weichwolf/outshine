#include "OsmSourceAcquisition.h"
#include "OsmSourceAcquisitionState.h"

#include "OsmChunkSetLoader.h"
#include "OsmApiReader.h"
#include "RawSourceCache.h"
#include "OsmProviderValidation.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>
#include <chrono>
#include <Units.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kMaxChunks = 4;
}

SourceAcquisition::SourceAcquisition(Workers workers,
                                     Data::SourceTransport *wire,
                                     std::string cacheDirectory,
                                     std::shared_ptr<const CellCompiler> compiler)
    : Compiler_(std::move(compiler)),
      Tasks_(&workers.Compute),
      Io_(workers.Io),
      Access_(std::make_shared<Access>()) {
  Access_->Wire = wire;
  Access_->Directory = std::move(cacheDirectory);
}

SourceAcquisition::~SourceAcquisition() {
  CancelCellPipeline();
  if (Pending_) { (void)Pending_->Stop.request_stop(); }

  struct RetiredInputs {
    std::shared_ptr<Access> Acquisition;
    std::unique_ptr<Cells> Coverage;
    std::shared_ptr<CellPipeline> Pipeline;
    std::optional<Pending> Request;
    std::shared_ptr<const SourceSnapshot> Current;
    std::shared_ptr<const CellCompiler> Compiler;
  };

  const auto retired = std::make_shared<RetiredInputs>();
  retired->Acquisition = std::move(Access_);
  retired->Coverage = std::move(Cells_);
  retired->Pipeline = std::move(CellPipeline_);
  retired->Request = std::move(Pending_);
  retired->Current = std::move(Current_);
  retired->Compiler = Compiler_;
  (void)Io_.PostDetached([retired] { (void)retired; });
}

std::expected<void, std::string> SourceAcquisition::SetAcquisitionBudget(double seconds) {
  const double milliseconds = seconds * kMsPerS;
  if (!std::isfinite(milliseconds) || milliseconds < 0) {
    return std::unexpected("invalid original OSM acquisition budget");
  }
  Access_->AcquisitionBudgetMs.store(milliseconds, std::memory_order_relaxed);
  Access_->BudgetRevision.fetch_add(1, std::memory_order_release);
  return {};
}

std::expected<void, std::string>
SourceAcquisition::Request(std::span<const Data::SourceProvider> providers,
                           std::string_view root,
                           const Data::ProviderRegistry *registry) {
  if (auto valid = ValidateProviders(providers); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  if (providers.size() > kMaxChunks ||
      std::ranges::any_of(providers, [](const auto &provider) { return provider.Kind != "osm"; })) {
    return std::unexpected("original OSM source requires at most four osm chunks");
  }
  std::vector<Data::SourceProvider> requested(providers.begin(), providers.end());
  std::ranges::sort(requested, {}, &Data::SourceProvider::Priority);
  if (Scope_ == Scope::Region && requested == Requested_ && (requested.empty() || root == Root_) &&
      registry == Access_->Registry && Phase_ != Phase::Failed) {
    return {};
  }
  if (Revision_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected("original OSM source revision is exhausted");
  }
  ++Revision_;
  Scope_ = Scope::Region;
  if (Cells_) {
    Cells_->Roots.clear();
    Cells_->PublishedRoots.clear();
    Cells_->Wanted.clear();
    Cells_->Preparing.clear();
    Cells_->Published.clear();
    Cells_->PublishedProvider.reset();
  }
  Requested_ = std::move(requested);
  Root_ = root;
  Access_->Registry = registry;
  Error_.clear();
  if (Pending_) { (void)Pending_->Stop.request_stop(); }
  CancelCellPipeline();
  if (Requested_.empty()) {
    Current_.reset();
    PublishedRevision_ = Revision_;
    Phase_ = Phase::Inactive;
    return {};
  }
  Phase_ = Phase::Loading;
  Poll();
  return {};
}

void SourceAcquisition::Poll() {
  if (Pending_ && Pending_->Output->Finished.load(std::memory_order_acquire)) {
    Pending finished = std::move(*Pending_);
    Pending_.reset();
    CompletePending(std::move(finished));
  }
  if (CellPipeline_) { PumpCellPipeline(); }
  if (!Pending_ && !CellPipeline_ && Phase_ == Phase::Loading) {
    if (Scope_ == Scope::Cells) {
      StartCellPipeline();
      PumpCellPipeline();
    } else {
      StartRegionAcquisition();
    }
  }
}

bool SourceAcquisition::AwaitSlice(double seconds) {
  if ((!Pending_ && !CellPipeline_) || !(seconds > 0.0)) { return false; }
  auto &progress = *Access_->Progress;
  std::unique_lock lock(progress.Mutex);
  return progress.Changed.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
    if (Pending_ && Pending_->Output->Finished.load(std::memory_order_acquire)) { return true; }
    if (!CellPipeline_) { return false; }
    const auto &exchange = *CellPipeline_->Shared;
    return exchange.Finished.load(std::memory_order_acquire) || !exchange.Error.empty() ||
           (!Pending_ && !exchange.Ready.empty());
  });
}

void SourceAcquisition::CompletePending(Pending finished) {
  if (finished.Revision != Revision_ || Phase_ != Phase::Loading) { return; }
  if (auto *read = std::get_if<ReadResult>(&finished.Output->Value)) {
    if (*read) {
      StartDecode(std::move(**read), std::move(finished.Stop), finished.Output->ReadMs);
      return;
    }
    Error_ = std::move(read->error());
  } else if (auto *loaded = std::get_if<LoadResult>(&finished.Output->Value)) {
    if (*loaded) {
      Current_ = std::move(**loaded);
      PublishedRevision_ = Revision_;
      Phase_ = Phase::Ready;
      Error_.clear();
      return;
    }
    Error_ = std::move(loaded->error());
  } else if (auto *cells = std::get_if<CellLoadResult>(&finished.Output->Value)) {
    if (*cells) {
      ReleaseAssignedCells(**cells);
      CompleteCells(std::move(**cells));
      return;
    }
    Error_ = std::move(cells->error());
  } else {
    Error_ = "original OSM worker returned no result";
  }
  if (Scope_ == Scope::Cells) { Cells_->Preparing.clear(); }
  Phase_ = Phase::Failed;
}

void SourceAcquisition::StartRegionAcquisition() {
  auto result = std::make_shared<Result>();
  result->Progress = Access_->Progress;
  std::stop_source stop;
  const auto token = stop.get_token();
  const auto complete = std::make_shared<AcquisitionCompletion<Result>>(result);
  const bool accepted = Io_.PostDetached([input = Requested_,
                                          revision = Revision_,
                                          root = Root_,
                                          access = Access_,
                                          registry = Access_->Registry,
                                          complete,
                                          token] {
    auto &output = *complete->Owner;
    if (token.stop_requested()) {
      output.Value = ReadResult(std::unexpected("original OSM acquisition canceled"));
      return;
    }
    const bool remote = registry != nullptr || std::ranges::any_of(input, [](const auto &provider) {
                          return !provider.Endpoint.empty();
                        });
    if (remote && !access->Wire) {
      output.Value = ReadResult(std::unexpected("official original OSM needs a source transport"));
      return;
    }
    if (remote) {
      if (!access->Store) {
        access->Store = std::make_unique<Data::RawSourceCache>(
            Data::RawSourceCache::Config{.Directory = access->Directory});
      }
      const auto deadline = [access, revision] { return access->CurrentDeadline(revision); };
      auto read = ReadRegions(
          input, *access->Store, *access->Wire, deadline(), token, registry, root, deadline);
      if (read) {
        output.ReadMs = read->ElapsedMs;
        output.Value = ReadResult(std::move(read->Chunks));
      } else {
        output.Value = ReadResult(std::unexpected(std::move(read.error())));
      }
    } else {
      output.Value = ChunkSetLoader::ReadRegion(input, root, token);
    }
  });
  if (!accepted) { result->Value = ReadResult(std::unexpected("original OSM IO queue is closed")); }
  Pending_.emplace(
      Pending{.Revision = Revision_, .Output = std::move(result), .Stop = std::move(stop)});
}

void SourceAcquisition::StartDecode(std::vector<SourceChunk> input,
                                    std::stop_source stop,
                                    std::optional<double> readMs) {
  auto result = std::make_shared<Result>();
  result->Progress = Access_->Progress;
  const auto token = stop.get_token();
  const bool cells = Scope_ == Scope::Cells;
  const auto complete = std::make_shared<AcquisitionCompletion<Result>>(result);
  const bool accepted = Tasks_->PostDetached(
      [input = std::move(input), complete, token, readMs, cells, compiler = Compiler_] {
        auto &output = *complete->Owner;
        if (cells) {
          std::vector<CellSource> ready;
          ready.reserve(input.size());
          for (const auto &chunk : input) {
            auto cell = DecodeCell(chunk, token, compiler);
            if (!cell) {
              output.Value = CellLoadResult(std::unexpected(std::move(cell.error())));
              return;
            }
            ready.push_back(std::move(*cell));
          }
          output.Value = CellLoadResult(std::move(ready));
          return;
        }
        auto loaded = ChunkSetLoader::ParseRegion(input, token);
        if (!loaded) {
          output.Value = LoadResult(std::unexpected(std::move(loaded.error())));
        } else {
          if (readMs) { loaded->ReadMs = *readMs; }
          output.Value = LoadResult(std::make_shared<const SourceSnapshot>(std::move(*loaded)));
        }
      });
  if (!accepted) {
    result->Value = LoadResult(std::unexpected("original OSM compute queue is closed"));
  }
  Pending_.emplace(
      Pending{.Revision = Revision_, .Output = std::move(result), .Stop = std::move(stop)});
}

}
