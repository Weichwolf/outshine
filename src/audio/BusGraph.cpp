#include "BusGraph.h"

#include <cmath>

namespace outshine::Audio {

namespace {

// [SET] the pool bounds: a scenario's buses are a declared handful and its sounds a
// declared few hundred -- both stand up once, and the mix walks them without allocating
constexpr size_t kMostBuses = 64;
constexpr size_t kMostSounds = 1024;

[[nodiscard]] double Linear(double gainDb) { return std::pow(10.0, gainDb / 20.0); }

} // namespace

int BusGraph::BusNamed(std::string_view id) const {
  for (size_t at = 0; at < Buses_.size(); ++at) {
    if (Buses_[at].Id == id) { return (int)at; }
  }
  return -1;
}

bool BusGraph::Build(std::span<const Bus> buses, std::span<const Sound> sounds,
                     std::string &error) {
  Buses_.clear();
  Sounds_.clear();
  Master_ = -1;
  Voices_ = 0;

  if (buses.size() > kMostBuses) {
    error = "the scenario declares " + std::to_string(buses.size()) +
            " buses over the pool's " + std::to_string(kMostBuses);
    return false;
  }
  if (sounds.size() > kMostSounds) {
    error = "the scenario declares " + std::to_string(sounds.size()) +
            " sounds over the pool's " + std::to_string(kMostSounds);
    return false;
  }
  for (const Bus &bus : buses) {
    if (bus.Id.empty()) {
      error = "a bus without an id routes nothing, because nothing can name it";
      return false;
    }
    if (BusNamed(bus.Id) >= 0) {
      error = "the bus '" + bus.Id + "' is declared twice, and routing into it would be a "
              "coin toss";
      return false;
    }
    Buses_.push_back(Row{bus.Id, -1, Linear(bus.GainDb)});
  }
  for (size_t at = 0; at < buses.size(); ++at) {
    if (buses[at].Into.empty()) {
      if (Master_ >= 0) {
        error = "the buses '" + Buses_[(size_t)Master_].Id + "' and '" + Buses_[at].Id +
                "' both route into nothing, and a mix has ONE master";
        return false;
      }
      Master_ = (int)at;
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
  // every bus walks to the master in fewer steps than there are buses -- a cycle never
  // arrives, and the mix that walks this graph must terminate on the audio thread
  for (size_t at = 0; at < Buses_.size(); ++at) {
    size_t steps = 0;
    for (int walk = (int)at; walk >= 0; walk = Buses_[(size_t)walk].Into) {
      if (++steps > Buses_.size()) {
        error = "the bus '" + Buses_[at].Id +
                "' never reaches the master -- its route is a cycle, and a cycle on the "
                "audio thread is a hang between two buffers";
        return false;
      }
    }
  }

  for (const Sound &sound : sounds) {
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
      error = "the sound '" + sound.Id + "' routes into '" + sound.Bus +
              "', which no bus declares";
      return false;
    }
    if (sound.Positional && !(sound.FalloffM > 0.0)) {
      error = "the sound '" + sound.Id +
              "' is positional and declares no falloffM -- a positional source without a "
              "distance is a stereo source wearing a costume";
      return false;
    }
    Sounds_.push_back(
        Source{sound.Id, into, Linear(sound.GainDb), sound.FalloffM, sound.Positional});
  }
  return true;
}

std::string_view BusGraph::Master() const {
  return Master_ >= 0 ? std::string_view(Buses_[(size_t)Master_].Id) : std::string_view();
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
    for (int walk = sound.Into; walk >= 0; walk = Buses_[(size_t)walk].Into) {
      gain *= Buses_[(size_t)walk].Gain;
    }
    return gain;
  }
  return 0.0;
}

double BusGraph::GainAt(std::string_view id, const double sourceM[3],
                        const double listenerM[3]) const {
  for (const Source &sound : Sounds_) {
    if (sound.Id != id) { continue; }
    double gain = GainOf(id);
    if (!sound.Positional) { return gain; }
    double away = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double gap = sourceM[axis] - listenerM[axis];
      away += gap * gap;
    }
    away = std::sqrt(away);
    // the declared falloff is the distance at which the source is half as loud: the
    // inverse law 1/(1 + d/falloff) reaches 0.5 exactly there and never divides by zero
    return gain / (1.0 + away / sound.FalloffM);
  }
  return 0.0;
}

} // namespace outshine::Audio
