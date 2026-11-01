#include "EngineState.h"
#include "OfflineSourceTransport.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
using Generators::Weather::WeatherSnapshotLoader;

Holds<std::optional<Data::SourceSession::Configuration>>
Engine::State::PrepareWorldSources(const Scenario::Document &scenario) const {
  std::vector<Data::SourceProvider> providers;
  providers.reserve(scenario.Providers.size());
  for (const auto &provider : scenario.Providers) {
    if (provider.Kind != "osm") { providers.push_back(provider); }
  }
  if (providers.empty() && !scenario.Ground.Declared) {
    return std::optional<Data::SourceSession::Configuration>{};
  }
  auto prepared = Data::SourceSession::Prepare(
      providers, Session.Under.BuiltinDataDirectory, Services.Providers);
  if (!prepared) { return std::unexpected(std::move(prepared.error())); }
  return std::optional<Data::SourceSession::Configuration>{std::move(*prepared)};
}

bool Engine::State::ConfigureWorldSources(
    std::vector<Data::SourceProvider> &providers,
    std::optional<Data::SourceSession::Configuration> sources) {
  if (providers.empty() && !Session.Declared.Ground.Declared) { return true; }
  assert(sources);
  auto opened = Data::SourceSession::Open({.Directory = Session.Under.SourceCacheDirectory},
                                          std::move(*sources));
  if (!opened) {
    Error = opened.error();
    return false;
  }
  World.Sources = std::move(*opened);
  bool weather = false;
  for (size_t at = 0; at < World.Sources->Sources().Count(); ++at) {
    weather =
        weather || World.Sources->Sources().At(at).Declaration().Kind == Data::DataKind::Weather;
  }
  if (!weather) { return true; }
  if (!Picture.Shown.UtcS) {
    Error = "weather selection requires the resolved declaration UTC";
    return false;
  }
  if (!Services.Transport) {
    Services.Transport = Session.Under.Offline
                             ? std::unique_ptr<Data::SourceTransport>(
                                   std::make_unique<Data::OfflineSourceTransport>())
                             : std::unique_ptr<Data::SourceTransport>(
                                   std::make_unique<HttpTransport>(HttpTransport::Config{}));
  }
  if (!Services.Compute) { Services.Compute = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  if (!Services.Io) { Services.Io = std::make_unique<Tasks>(1); }
  World.Weather = std::make_unique<WeatherSnapshotLoader>(
      World.Sources,
      *Services.Transport,
      WeatherSnapshotLoader::Workers{.Io = *Services.Io, .Compute = *Services.Compute},
      *Picture.Shown.UtcS);
  return true;
}

bool Engine::State::PollWeather() {
  if (!World.Weather) { return true; }
  if (World.Weather->Advance() != WeatherSnapshotLoader::Phase::Failed) { return true; }
  Error = World.Weather->Error();
  return false;
}

std::string_view Engine::State::WeatherBlocker() const noexcept {
  if (!World.Weather) { return {}; }
  switch (World.Weather->CurrentPhase()) {
    case WeatherSnapshotLoader::Phase::Ready:
    case WeatherSnapshotLoader::Phase::Missing: return {};
    case WeatherSnapshotLoader::Phase::Failed: return World.Weather->Error();
    case WeatherSnapshotLoader::Phase::Cancelled: return "weather acquisition cancelled";
    default: return "native weather snapshot pending";
  }
}

Holds<std::shared_ptr<const WeatherSnapshot>> Engine::weather() const {
  if (const auto blocker = S_->WeatherBlocker(); !blocker.empty()) {
    return std::unexpected(std::string(blocker));
  }
  return S_->World.Weather ? S_->World.Weather->Snapshot() : nullptr;
}
}
