#include "Mixer.h"

#include <utility>
#include <expected>
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#include "math/Units.h"
#include "format/Number.h"

namespace outshine::Audio {

constexpr double kDecadeBase = 10.0;

constexpr double kDopplerLeast = 0.25;
constexpr double kDopplerMost = 4.0;
constexpr uint32_t kLcgWord = 1664525u;
constexpr uint32_t kLcgOffset = 1013904223u;
constexpr unsigned kNoiseDrop = 8u;
constexpr double kNoiseHalfSteps = 8388608.0;
constexpr double kRt60Decades = -3.0;

namespace {

namespace Says {
constexpr auto InvalidParameter = "invalid audio parameter";
constexpr auto DelayBudget = "audio delay exceeds the sample budget";
constexpr auto InvalidRate = "audio sample rate must be positive";
}

[[nodiscard]] double Shaped(std::string_view shape, double phase) {
  const double turn = phase - std::floor(phase);
  if (shape == "square") { return turn < 0.5 ? 1.0 : -1.0; }
  if (shape == "saw") { return 2.0 * turn - 1.0; }
  if (shape == "triangle") { return 4.0 * std::fabs(turn - 0.5) - 1.0; }
  return std::sin(2.0 * kPi * turn);
}

struct Running {
  double FrequencyHz = 440.0;
  double Gain = 1.0;
  double Feedback = 0.0;
  std::string Shape = "sine";
  double Phase = 0.0;
  double One = 0.0, Two = 0.0;
  std::vector<double> Ring;
  size_t At = 0;
  uint32_t Seed = 0x9E3779B9u;
};

constexpr double kDefaultFilterFrequencyHz = 1000.0;
constexpr double kDefaultDelaySeconds = 0.05;
constexpr size_t kDelaySampleBudget = size_t{8} * 1024 * 1024;

[[nodiscard]] std::expected<Running, std::string>
PrepareVoice(const Scenario::Voice &voice, int rate, size_t &remainingSamples) {
  Running result;
  if (voice.Does == Scenario::Makes::Biquad) { result.FrequencyHz = kDefaultFilterFrequencyHz; }
  double delaySeconds = kDefaultDelaySeconds;
  for (const auto &parameter : voice.Parameters) {
    if (parameter.Name == "shape") {
      result.Shape = parameter.Value;
      continue;
    }
    const auto parsed = ParseFiniteNumber(parameter.Value);
    if (!parsed) { return std::unexpected(Says::InvalidParameter); }
    if (parameter.Name == "frequency") {
      result.FrequencyHz = *parsed;
    } else if (parameter.Name == "gain") {
      result.Gain = *parsed;
    } else if (parameter.Name == "feedback") {
      result.Feedback = *parsed;
    } else if (parameter.Name == "delayS") {
      delaySeconds = *parsed;
    } else {
      return std::unexpected(Says::InvalidParameter);
    }
  }
  if (result.FrequencyHz < 0 || delaySeconds < 0 || std::abs(result.Feedback) >= 1 ||
      (result.Shape != "sine" && result.Shape != "square" && result.Shape != "saw" &&
       result.Shape != "triangle")) {
    return std::unexpected(Says::InvalidParameter);
  }
  if (voice.Does == Scenario::Makes::Delay) {
    const double samples = delaySeconds * rate;
    if (!std::isfinite(samples) || samples >= static_cast<double>(remainingSamples)) {
      return std::unexpected(Says::DelayBudget);
    }
    const size_t count = static_cast<size_t>(samples) + 1;
    result.Ring.assign(count, 0.0);
    remainingSamples -= count;
  }
  return result;
}

[[nodiscard]] double Falloff(const Scenario::Emitter &heard, double awayM) {
  const double refM = heard.RefM > 0.0 ? heard.RefM : 1.0;
  const double atM = awayM < refM ? refM : awayM;
  if (heard.By == Scenario::Falls::Linear) {
    const double mostM = heard.MostM > refM ? heard.MostM : refM * 2.0;
    const double held = 1.0 - heard.Rolloff * (atM - refM) / (mostM - refM);
    return std::clamp(held, 0.0, 1.0);
  }
  if (heard.By == Scenario::Falls::Exponential) { return std::pow(atM / refM, -heard.Rolloff); }
  return refM / (refM + heard.Rolloff * (atM - refM));
}

[[nodiscard]] double Doppler(
    const Heard &source, const Listening &ear, const Vec3 &awayXyz, double awayM, double speedMs) {
  if (!(awayM > 0.0) || !(speedMs > 0.0)) { return 1.0; }
  double earToward = 0.0;
  double sourceAway = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    earToward += ear.VelocityMs[axis] * awayXyz[axis] / awayM;
    sourceAway += source.VelocityMs[axis] * awayXyz[axis] / awayM;
  }
  const double under = speedMs + sourceAway;
  if (!(under > 0.0)) { return 1.0; }
  const double shift = (speedMs + earToward) / under;
  return std::clamp(shift, kDopplerLeast, kDopplerMost);
}

struct Voicing {
  double Pitch = 1.0;
  int Rate = 0;
};

void Voiced(const Scenario::Sound &sound,
            std::vector<Running> &state,
            Voicing how,
            std::vector<double> &into) {
  const double pitch = how.Pitch;
  const int rate = how.Rate;
  const size_t frames = into.size();
  for (double &one : into) { one = 0.0; }
  if (sound.Graph.empty()) { return; }
  std::vector<std::vector<double>> made(sound.Graph.size(), std::vector<double>(frames, 0.0));
  std::unordered_map<std::string, size_t> named;
  for (size_t at = 0; at < sound.Graph.size(); ++at) { named[sound.Graph[at].Id] = at; }

  for (size_t at = 0; at < sound.Graph.size(); ++at) {
    const Scenario::Voice &makes = sound.Graph[at];
    Running &kept = state[at];
    std::vector<double> &out = made[at];
    std::vector<double> in(frames, 0.0);
    for (const std::string &from : makes.From) {
      const auto found = named.find(from);
      if (found == named.end() || found->second >= at) { continue; }
      for (size_t frame = 0; frame < frames; ++frame) { in[frame] += made[found->second][frame]; }
    }

    switch (makes.Does) {
      case Scenario::Makes::Oscillator: {
        const double hz = kept.FrequencyHz * pitch;
        for (size_t frame = 0; frame < frames; ++frame) {
          out[frame] = Shaped(kept.Shape, kept.Phase);
          kept.Phase += hz / static_cast<double>(rate);
          if (kept.Phase >= 1.0) { kept.Phase -= std::floor(kept.Phase); }
        }
        break;
      }
      case Scenario::Makes::Noise:
        for (size_t frame = 0; frame < frames; ++frame) {
          kept.Seed = kept.Seed * kLcgWord + kLcgOffset;
          out[frame] = static_cast<double>(kept.Seed >> kNoiseDrop) / kNoiseHalfSteps - 1.0;
        }
        break;
      case Scenario::Makes::Gain: {
        const double by = kept.Gain;
        for (size_t frame = 0; frame < frames; ++frame) { out[frame] = in[frame] * by; }
        break;
      }
      case Scenario::Makes::Biquad: {
        const double hz = kept.FrequencyHz;
        const double alpha = 1.0 - std::exp(-2.0 * kPi * hz / static_cast<double>(rate));
        for (size_t frame = 0; frame < frames; ++frame) {
          kept.One += alpha * (in[frame] - kept.One);
          out[frame] = kept.One;
        }
        break;
      }
      case Scenario::Makes::Delay: {
        const double back = kept.Feedback;
        for (size_t frame = 0; frame < frames; ++frame) {
          out[frame] = kept.Ring[kept.At];
          kept.Ring[kept.At] = in[frame] + out[frame] * back;
          kept.At = (kept.At + 1) % kept.Ring.size();
        }
        break;
      }
      case Scenario::Makes::Mix: out = in; break;
      default: break;
    }
  }
  into = made.back();
}

}

namespace {
struct Reverberation {
  std::vector<std::vector<double>> Combs;
  std::vector<size_t> CombAt;
  std::vector<double> CombBack;
  std::vector<double> CombKept;
  std::vector<std::vector<double>> Passes;
  std::vector<size_t> PassAt;
  double Damping = 0.5;
  double WetShare = 0.0;
  bool Standing = false;
};
}

struct Mixer::Held {
  void ConfigureRoom(std::span<const Scenario::Bus> buses, int rate);
  [[nodiscard]] bool
  BuildSources(std::span<const Scenario::Sound> declared, int rate, std::string &error);

  BusGraph Routing;
  std::vector<Scenario::Sound> Declared;
  std::vector<std::vector<Running>> State;
  std::vector<double> Scratch;
  std::vector<double> Dulled;
  std::vector<double> Wet;
  Reverberation Room;
  size_t Voices = 0;
};

void Mixer::Held::ConfigureRoom(std::span<const Scenario::Bus> buses, int rate) {
  for (const Scenario::Bus &one : buses) {
    if (!one.Reverberates.Declared || !(one.Reverberates.SecondsRt60 > 0.0)) { continue; }
    Room = Reverberation{};
    Room.Standing = true;
    Room.Damping = one.Reverberates.Damping;
    Room.WetShare = one.Reverberates.WetShare;
    constexpr std::array<int, 4> kCombs = {{1116, 1188, 1277, 1356}};
    constexpr std::array<int, 2> kPasses = {{556, 441}};
    for (const int held : kCombs) {
      const auto taps =
          static_cast<size_t>(static_cast<double>(held) * static_cast<double>(rate) / 44100.0);
      Room.Combs.emplace_back(taps == 0 ? 1u : taps, 0.0);
      Room.CombAt.push_back(0);
      Room.CombKept.push_back(0.0);
      const double delayS =
          static_cast<double>(Room.Combs.back().size()) / static_cast<double>(rate);
      Room.CombBack.push_back(
          std::pow(kDecadeBase, kRt60Decades * delayS / one.Reverberates.SecondsRt60));
    }
    for (const int held : kPasses) {
      const auto taps =
          static_cast<size_t>(static_cast<double>(held) * static_cast<double>(rate) / 44100.0);
      Room.Passes.emplace_back(taps == 0 ? 1u : taps, 0.0);
      Room.PassAt.push_back(0);
    }
    break;
  }
}

bool Mixer::Held::BuildSources(std::span<const Scenario::Sound> declared,
                               int rate,
                               std::string &error) {
  size_t remainingSamples = kDelaySampleBudget;
  for (const Scenario::Sound &one : declared) {
    if (one.Graph.empty() && one.Uri.empty() && !one.Streamed) {
      error = "the sound '" + one.Id +
              "' comes by no way at all -- a source is a file, a graph or a buffer a client "
              "fills, and declaring none of the three names nothing";
      return false;
    }
    for (const Scenario::Voice &makes : one.Graph) {
      if (makes.Does == Scenario::Makes::Convolver || makes.Does == Scenario::Makes::Shaper) {
        error = "the sound '" + one.Id + "' declares a '" +
                (makes.Does == Scenario::Makes::Convolver ? std::string("convolver")
                                                          : std::string("shaper")) +
                "' and this mixer does not run one yet -- a declared unit that silently does "
                "nothing is worse than a refusal, because the mix would sound finished";
        return false;
      }
    }
    auto &states = State.emplace_back();
    states.reserve(one.Graph.size());
    for (const auto &voice : one.Graph) {
      auto prepared = PrepareVoice(voice, rate, remainingSamples);
      if (!prepared) {
        error = std::move(prepared.error());
        return false;
      }
      states.push_back(std::move(*prepared));
    }
    Dulled.push_back(0.0);
    Voices += one.Graph.empty() ? 0 : 1;
  }
  return true;
}

Mixer::Mixer() : Held_(std::make_unique<Held>()) {}

Mixer::~Mixer() = default;
Mixer::Mixer(Mixer &&) noexcept = default;
Mixer &Mixer::operator=(Mixer &&) noexcept = default;

size_t Mixer::Voices() const {
  return Held_->Voices;
}

const BusGraph &Mixer::Routing() const {
  return Held_->Routing;
}

std::expected<void, std::string> Mixer::Stands(std::span<const Scenario::Bus> buses,
                                               std::span<const Scenario::Sound> declared,
                                               int rate) {
  if (rate <= 0) { return std::unexpected(Says::InvalidRate); }
  auto candidate = std::make_unique<Held>();
  auto routing = candidate->Routing.Build(buses, declared);
  if (!routing) { return routing; }
  candidate->Declared.assign(declared.begin(), declared.end());
  std::string error;
  if (!candidate->BuildSources(declared, rate, error)) { return std::unexpected(std::move(error)); }
  candidate->ConfigureRoom(buses, rate);
  Held_ = std::move(candidate);
  Rate_ = rate;
  return {};
}

bool Mixer::Fills(std::span<float> stereo,
                  std::span<const Heard> sources,
                  const Listening &ear,
                  std::string &error) {
  if (stereo.size() % 2 != 0) {
    error = "a stereo buffer holds an even number of samples and this one holds " +
            std::to_string(stereo.size());
    return false;
  }
  for (float &one : stereo) { one = 0.0f; }
  const size_t frames = stereo.size() / 2;
  Held_->Scratch.assign(frames, 0.0);
  Held_->Wet.assign(frames, 0.0);

  for (size_t at = 0; at < Held_->Declared.size(); ++at) {
    const Scenario::Sound &sound = Held_->Declared[at];
    if (sound.Graph.empty()) { continue; }
    const Heard *standing = nullptr;
    for (const Heard &one : sources) {
      if (one.Id == sound.Id && one.Standing) { standing = &one; }
    }
    if (standing == nullptr) { continue; }

    double gain = Held_->Routing.GainOf(sound.Id);
    double pitch = 1.0;
    double dullHz = 0.0;
    double leftShare = 0.5;
    double rightShare = 0.5;
    if (standing != nullptr && sound.Heard.Positional) {
      Vec3 awayXyz;
      double awayM = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        awayXyz[axis] = standing->AtM[axis] - ear.AtM[axis];
        awayM += awayXyz[axis] * awayXyz[axis];
      }
      awayM = std::sqrt(awayM);
      gain *= Falloff(sound.Heard, awayM);
      pitch = Doppler(*standing, ear, awayXyz, awayM, SpeedOfSoundMs_);
      const double along = awayM > 0.0
                               ? (awayXyz[0] * ear.RightXyz[0] + awayXyz[1] * ear.RightXyz[1] +
                                  awayXyz[2] * ear.RightXyz[2]) /
                                     awayM
                               : 0.0;
      rightShare = 0.5 * (1.0 + along);
      leftShare = 1.0 - rightShare;

      const double blocked = std::clamp(standing->Blocked, 0.0, 1.0);
      gain *= 1.0 + blocked * (sound.Heard.BlockedGain - 1.0);
      if (sound.Heard.BlockedHz > 0.0 && blocked > 0.0) { dullHz = sound.Heard.BlockedHz; }
    }
    if (!(gain > 0.0)) { continue; }

    Voiced(sound, Held_->State[at], {.Pitch = pitch, .Rate = Rate_}, Held_->Scratch);
    if (dullHz > 0.0) {
      const double alpha = 1.0 - std::exp(-2.0 * kPi * dullHz / static_cast<double>(Rate_));
      double &kept = Held_->Dulled[at];
      for (double &one : Held_->Scratch) {
        kept += alpha * (one - kept);
        one = kept;
      }
    }
    for (size_t frame = 0; frame < frames; ++frame) {
      const double one = Held_->Scratch[frame] * gain;
      stereo[frame * 2 + 0] += static_cast<float>(one * leftShare);
      stereo[frame * 2 + 1] += static_cast<float>(one * rightShare);
      Held_->Wet[frame] += one * sound.SendShare;
    }
  }

  if (Held_->Room.Standing) {
    Reverberation &room = Held_->Room;
    for (size_t frame = 0; frame < frames; ++frame) {
      double wet = 0.0;
      for (size_t comb = 0; comb < room.Combs.size(); ++comb) {
        std::vector<double> &ring = room.Combs[comb];
        const double heard = ring[room.CombAt[comb]];
        room.CombKept[comb] += (1.0 - room.Damping) * (heard - room.CombKept[comb]);
        ring[room.CombAt[comb]] = Held_->Wet[frame] + room.CombKept[comb] * room.CombBack[comb];
        room.CombAt[comb] = (room.CombAt[comb] + 1) % ring.size();
        wet += heard;
      }
      wet /= static_cast<double>(room.Combs.empty() ? 1u : room.Combs.size());
      for (size_t pass = 0; pass < room.Passes.size(); ++pass) {
        std::vector<double> &ring = room.Passes[pass];
        const double heard = ring[room.PassAt[pass]];
        ring[room.PassAt[pass]] = wet + heard * 0.5;
        wet = heard - wet;
        room.PassAt[pass] = (room.PassAt[pass] + 1) % ring.size();
      }
      stereo[frame * 2 + 0] += static_cast<float>(wet * room.WetShare);
      stereo[frame * 2 + 1] += static_cast<float>(wet * room.WetShare);
    }
  }
  return true;
}

}
