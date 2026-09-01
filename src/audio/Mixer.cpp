#include "Mixer.h"

#include <cmath>
#include <numbers>
#include <unordered_map>

#include "Units.h"

namespace outshine::Audio {

namespace {

[[nodiscard]] double
Named(std::span<const Setting> parameters, std::string_view name, double standing) {
  for (const Setting &one : parameters) {
    if (one.Name != name) { continue; }
    try {
      return std::stod(one.Value);
    } catch (...) { return standing; }
  }
  return standing;
}

[[nodiscard]] std::string
Spelt(std::span<const Setting> parameters, std::string_view name, std::string_view standing) {
  for (const Setting &one : parameters) {
    if (one.Name == name) { return one.Value; }
  }
  return std::string(standing);
}

[[nodiscard]] double Shaped(std::string_view shape, double phase) {
  const double turn = phase - std::floor(phase);
  if (shape == "square") { return turn < 0.5 ? 1.0 : -1.0; }
  if (shape == "saw") { return 2.0 * turn - 1.0; }
  if (shape == "triangle") { return 4.0 * std::fabs(turn - 0.5) - 1.0; }
  return std::sin(2.0 * kPi * turn);
}

struct Running {
  double Phase = 0.0;
  double One = 0.0, Two = 0.0;
  std::vector<double> Ring;
  size_t At = 0;
  uint32_t Seed = 0x9E3779B9u;
};

[[nodiscard]] double Falloff(const Emitter &heard, double awayM) {
  const double refM = heard.RefM > 0.0 ? heard.RefM : 1.0;
  const double atM = awayM < refM ? refM : awayM;
  if (heard.By == Falls::Linear) {
    const double mostM = heard.MostM > refM ? heard.MostM : refM * 2.0;
    const double held = 1.0 - heard.Rolloff * (atM - refM) / (mostM - refM);
    return held < 0.0 ? 0.0 : (held > 1.0 ? 1.0 : held);
  }
  if (heard.By == Falls::Exponential) { return std::pow(atM / refM, -heard.Rolloff); }
  return refM / (refM + heard.Rolloff * (atM - refM));
}

[[nodiscard]] double Doppler(const Heard &source,
                             const Listening &ear,
                             const double awayXyz[3],
                             double awayM,
                             double speedMs) {
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
  return shift < 0.25 ? 0.25 : (shift > 4.0 ? 4.0 : shift);
}

void Voiced(const Sound &sound,
            std::vector<Running> &state,
            double pitch,
            int rate,
            std::vector<double> &into) {
  const size_t frames = into.size();
  for (double &one : into) { one = 0.0; }
  if (sound.Graph.empty()) { return; }
  std::vector<std::vector<double>> made(sound.Graph.size(), std::vector<double>(frames, 0.0));
  std::unordered_map<std::string, size_t> named;
  for (size_t at = 0; at < sound.Graph.size(); ++at) { named[sound.Graph[at].Id] = at; }

  for (size_t at = 0; at < sound.Graph.size(); ++at) {
    const Voice &makes = sound.Graph[at];
    Running &kept = state[at];
    std::vector<double> &out = made[at];
    std::vector<double> in(frames, 0.0);
    for (const std::string &from : makes.From) {
      const auto found = named.find(from);
      if (found == named.end() || found->second >= at) { continue; }
      for (size_t frame = 0; frame < frames; ++frame) { in[frame] += made[found->second][frame]; }
    }

    switch (makes.Does) {
      case Makes::Oscillator: {
        const double hz = Named(makes.Parameters, "frequency", 440.0) * pitch;
        const std::string shape = Spelt(makes.Parameters, "shape", "sine");
        for (size_t frame = 0; frame < frames; ++frame) {
          out[frame] = Shaped(shape, kept.Phase);
          kept.Phase += hz / static_cast<double>(rate);
          if (kept.Phase >= 1.0) { kept.Phase -= std::floor(kept.Phase); }
        }
        break;
      }
      case Makes::Noise:
        for (size_t frame = 0; frame < frames; ++frame) {
          kept.Seed = kept.Seed * 1664525u + 1013904223u;
          out[frame] = static_cast<double>(kept.Seed >> 8) / 8388608.0 - 1.0;
        }
        break;
      case Makes::Gain: {
        const double by = Named(makes.Parameters, "gain", 1.0);
        for (size_t frame = 0; frame < frames; ++frame) { out[frame] = in[frame] * by; }
        break;
      }
      case Makes::Biquad: {
        const double hz = Named(makes.Parameters, "frequency", 1000.0);
        const double alpha = 1.0 - std::exp(-2.0 * kPi * hz / static_cast<double>(rate));
        for (size_t frame = 0; frame < frames; ++frame) {
          kept.One += alpha * (in[frame] - kept.One);
          out[frame] = kept.One;
        }
        break;
      }
      case Makes::Delay: {
        const double seconds = Named(makes.Parameters, "delayS", 0.05);
        const auto held = static_cast<size_t>(seconds * static_cast<double>(rate));
        if (kept.Ring.size() != held + 1) {
          kept.Ring.assign(held + 1, 0.0);
          kept.At = 0;
        }
        const double back = Named(makes.Parameters, "feedback", 0.0);
        for (size_t frame = 0; frame < frames; ++frame) {
          out[frame] = kept.Ring[kept.At];
          kept.Ring[kept.At] = in[frame] + out[frame] * back;
          kept.At = (kept.At + 1) % kept.Ring.size();
        }
        break;
      }
      case Makes::Mix: out = in; break;
      default: break;
    }
  }
  into = made.back();
}

} // namespace

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

struct Mixer::Held {
  BusGraph Routing;
  std::vector<Sound> Declared;
  std::vector<std::vector<Running>> State;
  std::vector<double> Scratch;
  std::vector<double> Dulled;
  std::vector<double> Wet;
  Reverberation Room;
  size_t Voices = 0;
};

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

bool Mixer::Stands(std::span<const Bus> buses,
                   std::span<const Sound> declared,
                   int rate,
                   std::string &error) {
  if (rate <= 0) {
    error = "a mixer runs at a rate and " + std::to_string(rate) + " is not one";
    return false;
  }
  Rate_ = rate;
  if (!Held_->Routing.Build(buses, declared, error)) { return false; }
  Held_->Declared.assign(declared.begin(), declared.end());
  Held_->State.clear();
  Held_->Dulled.clear();
  Held_->Room = Reverberation{};
  for (const Bus &one : buses) {
    if (!one.Reverberates.Declared || !(one.Reverberates.SecondsRt60 > 0.0)) { continue; }
    Held_->Room = Reverberation{};
    Held_->Room.Standing = true;
    Held_->Room.Damping = one.Reverberates.Damping;
    Held_->Room.WetShare = one.Reverberates.WetShare;
    constexpr int kCombs[] = {1116, 1188, 1277, 1356};
    constexpr int kPasses[] = {556, 441};
    for (const int held : kCombs) {
      const auto taps =
          static_cast<size_t>(static_cast<double>(held) * static_cast<double>(rate) / 44100.0);
      Held_->Room.Combs.emplace_back(taps == 0 ? 1u : taps, 0.0);
      Held_->Room.CombAt.push_back(0);
      Held_->Room.CombKept.push_back(0.0);
      const double delayS =
          static_cast<double>(Held_->Room.Combs.back().size()) / static_cast<double>(rate);
      Held_->Room.CombBack.push_back(std::pow(10.0, -3.0 * delayS / one.Reverberates.SecondsRt60));
    }
    for (const int held : kPasses) {
      const auto taps =
          static_cast<size_t>(static_cast<double>(held) * static_cast<double>(rate) / 44100.0);
      Held_->Room.Passes.emplace_back(taps == 0 ? 1u : taps, 0.0);
      Held_->Room.PassAt.push_back(0);
    }
    break;
  }
  Held_->Voices = 0;
  for (const Sound &one : declared) {
    if (one.Graph.empty() && one.Uri.empty() && !one.Streamed) {
      error = "the sound '" + one.Id +
              "' comes by no way at all -- a source is a file, a graph or a buffer a client "
              "fills, and declaring none of the three names nothing";
      return false;
    }
    for (const Voice &makes : one.Graph) {
      if (makes.Does == Makes::Convolver || makes.Does == Makes::Shaper) {
        error =
            "the sound '" + one.Id + "' declares a '" +
            (makes.Does == Makes::Convolver ? std::string("convolver") : std::string("shaper")) +
            "' and this mixer does not run one yet -- a declared unit that silently does "
            "nothing is worse than a refusal, because the mix would sound finished";
        return false;
      }
    }
    Held_->State.emplace_back(one.Graph.size());
    Held_->Dulled.push_back(0.0);
    Held_->Voices += one.Graph.empty() ? 0 : 1;
  }
  return true;
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
    const Sound &sound = Held_->Declared[at];
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
      double awayXyz[3];
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

      const double blocked = standing->Blocked < 0.0   ? 0.0
                             : standing->Blocked > 1.0 ? 1.0
                                                       : standing->Blocked;
      gain *= 1.0 + blocked * (sound.Heard.BlockedGain - 1.0);
      if (sound.Heard.BlockedHz > 0.0 && blocked > 0.0) { dullHz = sound.Heard.BlockedHz; }
    }
    if (!(gain > 0.0)) { continue; }

    Voiced(sound, Held_->State[at], pitch, Rate_, Held_->Scratch);
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

} // namespace outshine::Audio
