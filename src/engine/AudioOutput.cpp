#include "EngineState.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine {

namespace Says {
constexpr auto kAudioAssemblyRequired =
    "assemble the current declaration before binding audio sources";
constexpr auto kAudioDeclarationRequired = "declare content before preparing audio";
constexpr auto kAudioPreparationRequired = "prepare audio before mixing";
}

Result Engine::prepareAudio(int sampleRateHz) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (!S_->Session.Taken) {
    S_->Error = Says::kAudioDeclarationRequired;
    return std::unexpected(S_->Error);
  }
  const bool bound =
      std::ranges::any_of(S_->Session.Declared.Sounds,
                          [](const Audio::SoundSource &sound) { return !sound.Body.empty(); });
  if (bound && S_->Simulation->DeclarationRevision != S_->Session.DeclarationRevision) {
    S_->Error = Says::kAudioAssemblyRequired;
    return std::unexpected(S_->Error);
  }
  Audio::Mixer candidate;
  auto setup =
      candidate.Configure(S_->Session.Declared.Buses, S_->Session.Declared.Sounds, sampleRateHz);
  if (!setup) {
    S_->Error = setup.error();
    return setup;
  }
  std::vector<std::optional<size_t>> bindings(S_->Session.Declared.Sounds.size());
  if (bound) {
    auto resolved = S_->Simulation->BindAudio(S_->Session.Declared.Sounds);
    if (!resolved) {
      S_->Error = resolved.error();
      return std::unexpected(S_->Error);
    }
    bindings = std::move(*resolved);
  }
  S_->Session.AudioBodies = std::move(bindings);
  S_->PublishAudioSnapshot();
  S_->Session.Sounding.emplace(std::move(candidate));
  S_->Error.clear();
  return {};
}

Result Engine::mix(std::span<float> stereo) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (!S_->Session.Sounding) {
    S_->Error = Says::kAudioPreparationRequired;
    return std::unexpected(S_->Error);
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  if (!S_->Session.Sounding->Mix(
          stereo, S_->Session.Sources[told], S_->Session.Ear[told], S_->Error)) {
    return std::unexpected(S_->Error);
  }
  S_->Error.clear();
  return {};
}

}
