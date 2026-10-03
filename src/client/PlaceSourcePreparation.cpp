#include "PlaceSourcePreparation.h"
#include "OsmSourceDemand.h"
#include "ShippedProviders.h"
#include "Fetching.h"
#include "OfflineTransport.h"
#include "Tasks.h"
#include "GeodeticCamera.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Client {

Result
PreparePlaceSources(const Scenario::Document &declared,
                    const Roots &roots,
                    double budgetS,
                    const std::function<void(Generators::Osm::SourceCacheProgress)> &progress) {
  const auto began = std::chrono::steady_clock::now();
  if (!std::isfinite(budgetS) || budgetS <= 0) {
    return std::unexpected("invalid place source preparation budget");
  }
  const auto catalogue = [](const Data::SourceProvider &provider) {
    return provider.Kind == "osm" && !provider.Coverage && provider.Location.empty();
  };
  if (!std::ranges::any_of(declared.Providers, catalogue)) { return {}; }
  if (declared.Views.size() != 1 ||
      declared.Views.front().Placement != Scenario::CameraPlacement::Geodetic) {
    return std::unexpected("place source preparation requires one geodetic station");
  }
  const auto &view = declared.Views.front();
  const auto &station = view.Geographic.Geodetic;
  LongitudeLatitude focus{.LongitudeDeg = station.LongitudeDeg, .LatitudeDeg = station.LatitudeDeg};
  const LongitudeLatitude origin{.LongitudeDeg = declared.Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = declared.Ground.Origin.LatitudeDeg};
  const auto eye = ResolveGeodeticCamera(view, origin, nullptr);
  if (eye && *eye) { focus = GeographicFocusFor(**eye, origin); }
  const double radiusM =
      declared.Ground.SightM > 0 ? declared.Ground.SightM : Scenario::kSightUnsaidM;
  auto cells = Data::CellsAround(focus.LatitudeDeg,
                                 focus.LongitudeDeg,
                                 radiusM,
                                 Generators::Osm::kCatalogueCellLevel,
                                 Generators::Osm::kDefaultCellLimits.CellsMost);
  if (!cells) { return std::unexpected(std::move(cells.error())); }
  Tasks compute(1);
  Tasks io(1);
  std::unique_ptr<Data::Transport> wire;
  if (roots.Offline) {
    wire = std::make_unique<Data::OfflineTransport>();
  } else {
    wire = std::make_unique<Fetching>(
        Fetching::Config{.MaxBodyBytes = Generators::Osm::kMaximumXmlBytes});
  }
  Data::ProviderRegistry registry;
  Generators::RegisterShippedProviders(registry);
  for (const auto &provider : declared.Providers) {
    if (!catalogue(provider)) { continue; }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    if (elapsed >= budgetS) {
      return std::unexpected("place source preparation deadline exceeded");
    }
    const auto tell = [&](Generators::Osm::SourceCacheProgress how) {
      how.ElapsedS += elapsed;
      if (progress) { progress(how); }
    };
    if (auto prepared =
            Generators::Osm::PrepareSourceCache({.Catalogue = provider,
                                                 .Cells = *cells,
                                                 .Limits = Generators::Osm::kDefaultCellLimits,
                                                 .ShippedRoot = roots.Shipped,
                                                 .CacheDirectory = roots.Cache,
                                                 .BudgetS = budgetS - elapsed},
                                                {.Compute = compute, .Io = io},
                                                *wire,
                                                registry,
                                                tell);
        !prepared) {
      return prepared;
    }
  }
  return {};
}

}
