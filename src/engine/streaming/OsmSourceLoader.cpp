#include "OsmSourceLoader.h"

#include "OsmChunkSetLoader.h"
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
#include <vector>

namespace outshine {
namespace {
constexpr size_t kMaxChunks = 4;
}

OsmSourceLoader::~OsmSourceLoader() {
  if (Pending_) {
    (void)Pending_->Stop.request_stop();
    Tasks_->Wait(Pending_->Handle);
  }
}

std::expected<void, std::string>
OsmSourceLoader::Request(std::span<const Data::SourceProvider> providers, std::string_view root) {
  if (auto valid = Data::ValidateSourceProviders(providers); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  if (providers.size() > kMaxChunks ||
      std::ranges::any_of(providers, [](const auto &provider) { return provider.Kind != "osm"; })) {
    return std::unexpected("original OSM source requires at most four osm chunks");
  }
  std::vector<Data::SourceProvider> requested(providers.begin(), providers.end());
  std::ranges::sort(requested, {}, &Data::SourceProvider::Priority);
  if (requested == Requested_ && (requested.empty() || root == Root_) && Phase_ != Phase::Failed) {
    return {};
  }
  if (Revision_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected("original OSM source revision is exhausted");
  }
  ++Revision_;
  Requested_ = std::move(requested);
  Root_ = root;
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
  if (Pending_ && Tasks_->Done(Pending_->Handle)) {
    const Pending finished = std::move(*Pending_);
    Pending_.reset();
    if (finished.Revision == Revision_) {
      if (!finished.Output->Value) {
        Error_ = "original OSM worker returned no result";
        Phase_ = Phase::Failed;
      } else if (!*finished.Output->Value) {
        Error_ = std::move(finished.Output->Value->error());
        Phase_ = Phase::Failed;
      } else {
        Current_ = std::move(**finished.Output->Value);
        Phase_ = Phase::Ready;
        Error_.clear();
      }
    }
  }
  if (!Pending_ && Phase_ == Phase::Loading) { StartRequested(); }
}

void OsmSourceLoader::StartRequested() {
  auto result = std::make_shared<Result>();
  std::stop_source stop;
  const auto token = stop.get_token();
  const auto handle = Tasks_->Post([input = Requested_, root = Root_, result, token] {
    auto loaded = Data::OsmChunkSetLoader::LoadRegion(input, root, token);
    if (!loaded) {
      result->Value = std::unexpected(std::move(loaded.error()));
    } else {
      result->Value = std::make_shared<const Data::OsmSourceSnapshot>(std::move(*loaded));
    }
  });
  Pending_.emplace(Pending{.Handle = handle,
                           .Revision = Revision_,
                           .Output = std::move(result),
                           .Stop = std::move(stop)});
}

}
