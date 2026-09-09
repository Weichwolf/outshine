#include "BusGraph.h"

#include <cmath>
#include <expected>
#include <utility>
#include <cstddef>
#include <string_view>
#include <span>
#include <string>

namespace outshine::Audio {

constexpr double kDecadeBase = 10.0;

constexpr double kDecibelsPerDecade = 20.0;

namespace {

namespace Says {
constexpr auto InvalidGain = "audio gain or routed gain exceeds finite amplitude storage";
constexpr auto PoolLimit = "audio graph exceeds its bus or sound capacity";
}

constexpr size_t kMostBuses = 64;
constexpr size_t kMostSounds = 1024;

[[nodiscard]] double Linear(double gainDb) {
  return std::pow(kDecadeBase, gainDb / kDecibelsPerDecade);
}

}

int BusGraph::BusNamed(std::string_view id) const {
  for (size_t at = 0; at < Buses_.size(); ++at) {
    if (Buses_[at].Id == id) { return static_cast<int>(at); }
  }
  return -1;
}

std::expected<void, std::string> BusGraph::Build(std::span<const Scenario::Bus> buses,
                                                 std::span<const Scenario::Sound> sounds) {
  if (buses.size() > kMostBuses || sounds.size() > kMostSounds) {
    return std::unexpected(Says::PoolLimit);
  }
  BusGraph candidate;
  std::string error;
  if (!candidate.DefineBuses(buses, error) || !candidate.RouteBuses(buses, error) ||
      !candidate.DefineSounds(sounds, error)) {
    return std::unexpected(std::move(error));
  }
  *this = std::move(candidate);
  return {};
}

bool BusGraph::DefineBuses(std::span<const Scenario::Bus> buses, std::string &error) {
  for (const Scenario::Bus &bus : buses) {
    if (bus.Id.empty()) {
      error = "a bus without an id routes nothing, because nothing can name it";
      return false;
    }
    if (BusNamed(bus.Id) >= 0) {
      error = "the bus '" + bus.Id +
              "' is declared twice, and routing into it would be a "
              "coin toss";
      return false;
    }
    if (!std::isfinite(bus.GainDb) || !std::isfinite(Linear(bus.GainDb))) {
      error = Says::InvalidGain;
      return false;
    }
    Buses_.push_back(Row{.Id = bus.Id, .Into = -1, .Gain = Linear(bus.GainDb)});
  }
  return true;
}

bool BusGraph::RouteBuses(std::span<const Scenario::Bus> buses, std::string &error) {
  for (size_t at = 0; at < buses.size(); ++at) {
    if (buses[at].Into.empty()) {
      if (Master_ >= 0) {
        error = "the buses '" + Buses_[static_cast<size_t>(Master_)].Id + "' and '" +
                Buses_[at].Id + "' both route into nothing, and a mix has ONE master";
        return false;
      }
      Master_ = static_cast<int>(at);
      continue;
    }
    const int into = BusNamed(buses[at].Into);
    if (into < 0) {
      error = "the bus '" + Buses_[at].Id + "' routes into '" + buses[at].Into +
              "', which no bus declares";
      return false;
    }
    Buses_[at].Into = into;
  }
  if (Buses_.empty()) {
    error = "a mix stands on 1..N buses and this scenario declares none";
    return false;
  }
  if (Master_ < 0) {
    error = "every declared bus routes into another, so the mix has no master and no sound "
            "leaves it";
    return false;
  }
  for (size_t at = 0; at < Buses_.size(); ++at) {
    size_t steps = 0;
    double gain = 1;
    for (int walk = static_cast<int>(at); walk >= 0;
         walk = Buses_[static_cast<size_t>(walk)].Into) {
      gain *= Buses_[static_cast<size_t>(walk)].Gain;
      if (!std::isfinite(gain)) {
        error = Says::InvalidGain;
        return false;
      }
      if (++steps > Buses_.size()) {
        error = "the bus '" + Buses_[at].Id +
                "' never reaches the master -- its route is a cycle, and a cycle on the "
                "audio thread is a hang between two buffers";
        return false;
      }
    }
  }

  return true;
}

bool BusGraph::DefineSounds(std::span<const Scenario::Sound> sounds, std::string &error) {
  for (const Scenario::Sound &sound : sounds) {
    if (sound.Id.empty()) {
      error = "a sound without an id cannot be played, and a sound nobody can play is "
              "dead weight";
      return false;
    }
    for (const Source &held : Sounds_) {
      if (held.Id == sound.Id) {
        error = "the sound '" + sound.Id + "' is declared twice";
        return false;
      }
    }
    const int into = sound.Bus.empty() ? Master_ : BusNamed(sound.Bus);
    if (into < 0) {
      error = "the sound '" + sound.Id + "' routes into '" + sound.Bus + "', which no bus declares";
      return false;
    }
    if (sound.Heard.Positional && (!std::isfinite(sound.Heard.RefM) || !(sound.Heard.RefM > 0.0))) {
      error = "the sound '" + sound.Id +
              "' is positional and its refM is not above zero -- a positional source without a "
              "distance is a stereo source wearing a costume";
      return false;
    }
    if (!std::isfinite(sound.GainDb) || !std::isfinite(Linear(sound.GainDb))) {
      error = Says::InvalidGain;
      return false;
    }
    Sounds_.push_back(Source{.Id = sound.Id,
                             .Into = into,
                             .Gain = Linear(sound.GainDb),
                             .Positional = sound.Heard.Positional});
    if (!std::isfinite(GainOf(sound.Id))) {
      error = Says::InvalidGain;
      return false;
    }
  }
  return true;
}

std::string_view BusGraph::Master() const {
  return Master_ >= 0 ? std::string_view(Buses_[static_cast<size_t>(Master_)].Id)
                      : std::string_view();
}

bool BusGraph::Play(std::string_view id, std::string &error) {
  for (const Source &sound : Sounds_) {
    if (sound.Id == id) {
      ++Voices_;
      return true;
    }
  }
  error = "the sound '" + std::string(id) +
          "' is not declared -- a missing sound is a refusal and never a silence, because "
          "a silence is what a working sound also sounds like";
  return false;
}

double BusGraph::GainOf(std::string_view id) const {
  for (const Source &sound : Sounds_) {
    if (sound.Id != id) { continue; }
    double gain = sound.Gain;
    for (int walk = sound.Into; walk >= 0; walk = Buses_[static_cast<size_t>(walk)].Into) {
      gain *= Buses_[static_cast<size_t>(walk)].Gain;
    }
    return gain;
  }
  return 0.0;
}

}
