#include "OsmSourceAcquisition.h"
#include "OsmSourceAcquisitionState.h"

#include "OsmChunkSetLoader.h"
#include "OsmApiReader.h"
#include "ContentStore.h"
#include "SourceProviderValidation.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <math/Units.h>
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
                                     Data::Transport *wire,
                                     std::string cacheDirectory)
    : Tasks_(&workers.Compute), Io_(workers.Io), Access_(std::make_shared<Access>()) {
  Access_->Wire = wire;
  Access_->Directory = std::move(cacheDirectory);
}

SourceAcquisition::~SourceAcquisition() {
  CancelCellPipeline();
  if (Pending_) {
    (void)Pending_->Stop.request_stop();
    Pending_->Owner->Wait(Pending_->Handle);
  }
  if (CellPipeline_) { Io_.Wait(CellPipeline_->Handle); }
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
  if (auto valid = Data::ValidateSourceProviders(providers); !valid) {
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
  if (Pending_ && Pending_->Owner->TakeCompletion(Pending_->Handle)) {
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
  if (CellPipeline_) { return Io_.AwaitCompletion(std::min(seconds, MaximumIoAwaitSeconds)); }
  return Pending_ && Pending_->Owner->AwaitCompletion(seconds);
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
  std::stop_source stop;
  const auto token = stop.get_token();
  const auto handle = Io_.Post([input = Requested_,
                                revision = Revision_,
                                root = Root_,
                                access = Access_,
                                registry = Access_->Registry,
                                result,
                                token] {
    const bool remote = registry != nullptr || std::ranges::any_of(input, [](const auto &provider) {
                          return !provider.Endpoint.empty();
                        });
    if (remote && !access->Wire) {
      result->Value = ReadResult(std::unexpected("official original OSM needs a source transport"));
      return;
    }
    if (remote) {
      if (!access->Store) {
        access->Store = std::make_unique<Data::ContentStore>(
            Data::ContentStore::Config{.Directory = access->Directory, .UtcSeconds = {}});
      }
      const auto deadline = [access, revision] { return access->CurrentDeadline(revision); };
      auto read = Data::ReadOsmApiRegions(
          input, *access->Store, *access->Wire, deadline(), token, registry, root, deadline);
      if (read) {
        result->ReadMs = read->ElapsedMs;
        result->Value = ReadResult(std::move(read->Chunks));
      } else {
        result->Value = ReadResult(std::unexpected(std::move(read.error())));
      }
    } else {
      result->Value = Data::OsmChunkSetLoader::ReadRegion(input, root, token);
    }
  });
  Pending_.emplace(Pending{.Handle = handle,
                           .Owner = &Io_,
                           .Revision = Revision_,
                           .Output = std::move(result),
                           .Stop = std::move(stop)});
}

void SourceAcquisition::StartDecode(std::vector<Data::OsmSourceChunk> input,
                                    std::stop_source stop,
                                    std::optional<double> readMs) {
  auto result = std::make_shared<Result>();
  const auto token = stop.get_token();
  const bool cells = Scope_ == Scope::Cells;
  const auto handle = Tasks_->Post([input = std::move(input), result, token, readMs, cells] {
    if (cells) {
      std::vector<CellSource> ready;
      ready.reserve(input.size());
      for (const auto &chunk : input) {
        auto loaded = Data::OsmChunkSetLoader::ParseCell(chunk, token);
        if (!loaded) {
          result->Value = CellLoadResult(std::unexpected(std::move(loaded.error())));
          return;
        }
        const size_t charged = loaded->StorageChargeBytes();
        ready.push_back(
            {.Snapshot = std::make_shared<const Data::OsmSourceSnapshot>(std::move(*loaded)),
             .ChargedBytes = charged});
      }
      result->Value = CellLoadResult(std::move(ready));
      return;
    }
    auto loaded = Data::OsmChunkSetLoader::ParseRegion(input, token);
    if (!loaded) {
      result->Value = LoadResult(std::unexpected(std::move(loaded.error())));
    } else {
      if (readMs) { loaded->ReadMs = *readMs; }
      result->Value =
          LoadResult(std::make_shared<const Data::OsmSourceSnapshot>(std::move(*loaded)));
    }
  });
  Pending_.emplace(Pending{.Handle = handle,
                           .Owner = Tasks_,
                           .Revision = Revision_,
                           .Output = std::move(result),
                           .Stop = std::move(stop)});
}

}
