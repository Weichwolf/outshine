#include "OsmSourceLoader.h"

#include "OsmChunkSetLoader.h"
#include "OsmApiReader.h"
#include "ContentStore.h"
#include "SourceProviderValidation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kMaxChunks = 4;
constexpr double kAcquireBudgetMs = 10000.0;
}

struct OsmSourceLoader::Access {
  Data::Transport *Wire = nullptr;
  std::string Directory;
  std::unique_ptr<Data::ContentStore> Store;
  const Data::ProviderRegistry *Registry = nullptr;
};

OsmSourceLoader::OsmSourceLoader(Tasks &tasks, Data::Transport *wire, std::string cacheDirectory)
    : Tasks_(&tasks), Access_(std::make_shared<Access>()) {
  Access_->Wire = wire;
  Access_->Directory = std::move(cacheDirectory);
}

OsmSourceLoader::~OsmSourceLoader() {
  if (Pending_) {
    (void)Pending_->Stop.request_stop();
    Pending_->Owner->Wait(Pending_->Handle);
  }
}

std::expected<void, std::string>
OsmSourceLoader::Request(std::span<const Data::SourceProvider> providers,
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
  if (requested == Requested_ && (requested.empty() || root == Root_) &&
      registry == Access_->Registry && Phase_ != Phase::Failed) {
    return {};
  }
  if (Revision_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected("original OSM source revision is exhausted");
  }
  ++Revision_;
  Requested_ = std::move(requested);
  Root_ = root;
  Access_->Registry = registry;
  Error_.clear();
  if (Pending_) { (void)Pending_->Stop.request_stop(); }
  if (Requested_.empty()) {
    Current_.reset();
    Phase_ = Phase::Inactive;
    return {};
  }
  Phase_ = Phase::Loading;
  Poll();
  return {};
}

void OsmSourceLoader::Poll() {
  if (Pending_ && Pending_->Owner->Done(Pending_->Handle)) {
    Pending finished = std::move(*Pending_);
    Pending_.reset();
    CompletePending(std::move(finished));
  }
  if (!Pending_ && Phase_ == Phase::Loading) { StartRequested(); }
}

bool OsmSourceLoader::AwaitSlice(double seconds) const {
  return Pending_ && Pending_->Owner->AwaitCompletion(seconds);
}

void OsmSourceLoader::CompletePending(Pending finished) {
  if (finished.Revision != Revision_) { return; }
  if (auto *read = std::get_if<ReadResult>(&finished.Output->Value)) {
    if (*read) {
      StartDecode(std::move(**read), std::move(finished.Stop));
      return;
    }
    Error_ = std::move(read->error());
  } else if (auto *loaded = std::get_if<LoadResult>(&finished.Output->Value)) {
    if (*loaded) {
      Current_ = std::move(**loaded);
      Phase_ = Phase::Ready;
      Error_.clear();
      return;
    }
    Error_ = std::move(loaded->error());
  } else {
    Error_ = "original OSM worker returned no result";
  }
  Phase_ = Phase::Failed;
}

void OsmSourceLoader::StartRequested() {
  auto result = std::make_shared<Result>();
  std::stop_source stop;
  const auto token = stop.get_token();
  const auto handle = Io_.Post([input = Requested_,
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
    Data::OsmChunkSetLoader::RemoteRead read;
    if (remote) {
      if (!access->Store) {
        access->Store = std::make_unique<Data::ContentStore>(
            Data::ContentStore::Config{.Directory = access->Directory, .UtcSeconds = {}});
      }
      const double deadlineMs = access->Wire->NowMs() + kAcquireBudgetMs;
      read = [access, deadlineMs, root, registry](const auto &provider, const auto &stopToken) {
        return Data::ReadOsmApiRegion(
            provider, *access->Store, *access->Wire, deadlineMs, stopToken, registry, root);
      };
    }
    result->Value = Data::OsmChunkSetLoader::ReadRegion(input, root, token, read);
  });
  Pending_.emplace(Pending{.Handle = handle,
                           .Owner = &Io_,
                           .Revision = Revision_,
                           .Output = std::move(result),
                           .Stop = std::move(stop)});
}

void OsmSourceLoader::StartDecode(std::vector<Data::OsmSourceChunk> input, std::stop_source stop) {
  auto result = std::make_shared<Result>();
  const auto token = stop.get_token();
  const auto handle = Tasks_->Post([input = std::move(input), result, token] {
    auto loaded = Data::OsmChunkSetLoader::ParseRegion(input, token);
    if (!loaded) {
      result->Value = LoadResult(std::unexpected(std::move(loaded.error())));
    } else {
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
