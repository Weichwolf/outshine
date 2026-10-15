#include "EngineHeld.h"
#include "ShippedProviders.h"
#include "OfflineTransport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
bool Engine::State::ConfigureWorldSources(std::vector<Data::SourceProvider> &providers) {
  if (providers.empty() && Session.Declared.Ground.Declared) {
    const auto defaults = Generators::ShippedProviders();
    providers.assign(defaults.begin(), defaults.end());
  }
  if (providers.empty()) {
    World.Weather.reset();
    World.WeatherUtcS.reset();
    World.Stack.Close();
    World.Sources.reset();
    World.SourceDeclarations.clear();
    return true;
  }
  if (!World.Sources || World.SourceDeclarations != providers) {
    auto sources = Data::SourceSession::Open(
        {.Directory = Session.Under.Cache}, providers, Session.Under.Shipped, World.Providers);
    if (!sources) {
      Error = sources.error();
      return false;
    }
    World.Weather.reset();
    World.WeatherUtcS.reset();
    World.Stack.Close();
    World.Sources = std::move(*sources);
    World.SourceDeclarations = providers;
  }
  bool weather = false;
  for (size_t at = 0; at < World.Sources->Sources().Count(); ++at) {
    weather =
        weather || World.Sources->Sources().At(at).Declaration().Kind == Data::DataKind::Weather;
  }
  if (!weather) {
    World.Weather.reset();
    World.WeatherUtcS.reset();
    return true;
  }
  if (!Picture.Shown.UtcS) {
    Error = "weather selection requires the resolved declaration UTC";
    return false;
  }
  if (!World.Wire) {
    World.Wire =
        Session.Under.Offline
            ? std::unique_ptr<Data::Transport>(std::make_unique<Data::OfflineTransport>())
            : std::unique_ptr<Data::Transport>(std::make_unique<Fetching>(Fetching::Config{}));
  }
  if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  if (!World.Io) { World.Io = std::make_unique<Tasks>(1); }
  const bool failed =
      World.Weather && (World.Weather->CurrentPhase() == WeatherPreparation::Phase::Failed ||
                        World.Weather->CurrentPhase() == WeatherPreparation::Phase::Cancelled);
  if (!World.Weather || World.WeatherUtcS != Picture.Shown.UtcS || failed) {
    World.Weather.reset();
    World.Weather = std::make_unique<WeatherPreparation>(
        World.Sources,
        *World.Wire,
        WeatherPreparation::Workers{.Io = *World.Io, .Compute = *World.Pool},
        *Picture.Shown.UtcS);
    World.WeatherUtcS = Picture.Shown.UtcS;
  }
  return true;
}

bool Engine::State::PollWeather() {
  if (!World.Weather) { return true; }
  if (!Simulation || Simulation->DeclarationRevision != Session.DeclarationRevision) {
    Error = "assemble the current declaration before acquiring its weather";
    return false;
  }
  if (World.Weather->Advance() != WeatherPreparation::Phase::Failed) { return true; }
  Error = World.Weather->Error();
  return false;
}

std::string_view Engine::State::WeatherBlocker() const noexcept {
  if (!World.Weather) { return {}; }
  if (!Simulation || Simulation->DeclarationRevision != Session.DeclarationRevision) {
    return "current weather declaration is not assembled";
  }
  switch (World.Weather->CurrentPhase()) {
    case WeatherPreparation::Phase::Ready:
    case WeatherPreparation::Phase::Missing: return {};
    case WeatherPreparation::Phase::Failed: return World.Weather->Error();
    case WeatherPreparation::Phase::Cancelled: return "weather acquisition cancelled";
    default: return "native weather snapshot pending";
  }
}

Holds<std::shared_ptr<const WeatherSnapshot>> Engine::weather() const {
  if (S_->Session.Taken &&
      (!S_->Simulation || S_->Simulation->DeclarationRevision != S_->Session.DeclarationRevision)) {
    return std::unexpected("assemble the current declaration before reading its weather");
  }
  if (const auto blocker = S_->WeatherBlocker(); !blocker.empty()) {
    return std::unexpected(std::string(blocker));
  }
  return S_->World.Weather ? S_->World.Weather->Snapshot() : nullptr;
}
}
