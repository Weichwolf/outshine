#include "OsmApiReader.h"

#include "DeclaredSources.h"
#include "SourceSet.h"

#include <cmath>
#include <expected>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Data {
namespace {
constexpr double kIoAwaitMs = 5.0;
}

std::expected<OsmSourceChunk, std::string> ReadOsmApiRegion(const SourceProvider &provider,
                                                            ContentStore &store,
                                                            Transport &wire,
                                                            double deadlineMs,
                                                            const std::stop_token &stop,
                                                            const ProviderRegistry *registry,
                                                            std::string_view shippedRoot) {
  auto source = MakeDeclaredSource(provider, shippedRoot, registry);
  if (!source) { return std::unexpected(std::move(source.error())); }
  if ((*source)->Declaration().Kind != DataKind::OriginalOsm ||
      (*source)->Declaration().Wire != WireFormat::OsmXml) {
    return std::unexpected("an original OSM provider must supply original OSM XML");
  }
  const std::string url = (*source)->Declaration().Endpoint;
  SourceSet sources(store);
  if (sources.Add(std::move(*source)) != SourceSet::Registration::Accepted) {
    return std::unexpected("original OSM source registration failed");
  }
  sources.Seal();
  auto query = sources.Ask(Fetch(DataKind::OriginalOsm, Address::Whole(0)));
  const double beganMs = wire.NowMs();
  for (;;) {
    const double nowMs = wire.NowMs();
    if (stop.stop_requested() || !std::isfinite(nowMs) || !std::isfinite(deadlineMs) ||
        nowMs >= deadlineMs) {
      SourceSet::Abandon(query, wire);
      return std::unexpected(stop.stop_requested()
                                 ? "original OSM source acquisition canceled"
                                 : "original OSM source acquisition deadline exceeded");
    }
    auto delivery = sources.Collect(query, wire);
    if (auto answer = delivery.Take()) {
      return OsmSourceChunk{.Provider = provider,
                            .Xml = std::string(answer->Bytes.begin(), answer->Bytes.end()),
                            .Origin = url,
                            .ReadMs = wire.NowMs() - beganMs,
                            .FromStore = sources.Counters().FromStore != 0};
    }
    if (delivery.Where() != Delivery::State::Pending) {
      const auto reason =
          delivery.Failure() ? delivery.Failure()->Reason : FetchFailureReason::ProviderRefused;
      SourceSet::Abandon(query, wire);
      return std::unexpected("original OSM source '" + url +
                             "' failed: " + std::string(Name(reason)));
    }
    (void)wire.Await(kIoAwaitMs);
  }
}

}
