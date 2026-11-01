#include "EngineState.h"
#include "OfflineSourceTransport.h"

#include <algorithm>
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
  if ((providers.empty() && !scenario.Ground.Declared) ||
      (World().Sources && World().SourceDeclarations == providers)) {
    return std::optional<Data::SourceSession::Configuration>{};
  }
  auto prepared = Data::SourceSession::Prepare(
      providers, Session.Under.BuiltinDataDirectory, Services.Providers);
  if (!prepared) { return std::unexpected(std::move(prepared.error())); }
  return std::optional<Data::SourceSession::Configuration>{std::move(*prepared)};
}

Holds<std::unique_ptr<WorldRegionState>> Engine::State::PrepareWorldResources(
    const Scenario::Document &declared,
    const Core::Declaration &picture,
    std::optional<Data::SourceSession::Configuration> &configuration) {
  auto world = std::make_unique<WorldRegionState>();
  std::vector<Data::SourceProvider> providers;
  providers.reserve(declared.Providers.size());
  for (const auto &provider : declared.Providers) {
    if (provider.Kind != "osm") { providers.push_back(provider); }
  }
  if (!configuration && World().SourceDeclarations == providers) {
    world->Sources = World().Sources;
    world->SourceDeclarations = World().SourceDeclarations;
  }
  if (!ConfigureSourceProviders(*world, declared, picture.UtcS, configuration)) {
    return std::unexpected(Error);
  }
  if (declared.Ground.Declared && !ConfigureWorldGround(*world, declared)) {
    return std::unexpected(Error);
  }
  return world;
}

bool Engine::State::ConfigureWorldSources(
    WorldRegionState &world,
    const Scenario::Document &declared,
    std::optional<int64_t> utc,
    std::vector<Data::SourceProvider> &providers,
    std::optional<Data::SourceSession::Configuration> &configuration) {
  if (providers.empty() && !declared.Ground.Declared) {
    world.Weather.reset();
    world.WeatherUtcS.reset();
    world.UrbanAreas.reset();
    world.Surface().Close();
    world.Sources.reset();
    world.SourceDeclarations.clear();
    return true;
  }

  if (!world.Sources || world.SourceDeclarations != providers) {
    auto sources =
        configuration ? Data::SourceSession::Open({.Directory = Session.Under.SourceCacheDirectory},
                                                  std::move(*configuration))
                      : Data::SourceSession::Open({.Directory = Session.Under.SourceCacheDirectory},
                                                  providers,
                                                  Session.Under.BuiltinDataDirectory,
                                                  Services.Providers);
    if (!sources) {
      Error = sources.error();
      return false;
    }
    world.Weather.reset();
    world.WeatherUtcS.reset();
    world.UrbanAreas.reset();
    world.Surface().Close();
    world.Sources = std::move(*sources);
    world.SourceDeclarations = providers;
  }
  bool weather = false;
  for (size_t at = 0; at < world.Sources->Sources().Count(); ++at) {
    weather =
        weather || world.Sources->Sources().At(at).Declaration().Kind == Data::DataKind::Weather;
  }
  if (!weather) {
    world.Weather.reset();
    world.WeatherUtcS.reset();
    return true;
  }
  if (!utc) {
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
  const bool failed =
      world.Weather && (world.Weather->CurrentPhase() == WeatherSnapshotLoader::Phase::Failed ||
                        world.Weather->CurrentPhase() == WeatherSnapshotLoader::Phase::Cancelled);
  if (!world.Weather || world.WeatherUtcS != utc || failed) {
    world.Weather.reset();
    world.Weather = std::make_unique<WeatherSnapshotLoader>(
        world.Sources,
        *Services.Transport,
        WeatherSnapshotLoader::Workers{.Io = *Services.Io, .Compute = *Services.Compute},
        *utc);
    world.WeatherUtcS = utc;
  }
  return true;
}

bool Engine::State::PollWeather() {
  if (!World().Weather) { return true; }
  if (!ScenarioState || ScenarioState->DeclarationRevision != Session.DeclarationRevision) {
    Error = "assemble the current declaration before acquiring its weather";
    return false;
  }
  if (World().Weather->Advance() != WeatherSnapshotLoader::Phase::Failed) { return true; }
  Error = World().Weather->Error();
  return false;
}

std::string_view Engine::State::WeatherBlocker() const noexcept {
  if (!World().Weather) { return {}; }
  if (!ScenarioState || ScenarioState->DeclarationRevision != Session.DeclarationRevision) {
    return "current weather declaration is not assembled";
  }
  switch (World().Weather->CurrentPhase()) {
    case WeatherSnapshotLoader::Phase::Ready:
    case WeatherSnapshotLoader::Phase::Missing: return {};
    case WeatherSnapshotLoader::Phase::Failed: return World().Weather->Error();
    case WeatherSnapshotLoader::Phase::Cancelled: return "weather acquisition cancelled";
    default: return "native weather snapshot pending";
  }
}

Holds<std::shared_ptr<const WeatherSnapshot>> Engine::weather() const {
  if (S_->Session.Taken && (!S_->ScenarioState || S_->ScenarioState->DeclarationRevision !=
                                                      S_->Session.DeclarationRevision)) {
    return std::unexpected("assemble the current declaration before reading its weather");
  }
  if (const auto blocker = S_->WeatherBlocker(); !blocker.empty()) {
    return std::unexpected(std::string(blocker));
  }
  return S_->World().Weather ? S_->World().Weather->Snapshot() : nullptr;
}
}
