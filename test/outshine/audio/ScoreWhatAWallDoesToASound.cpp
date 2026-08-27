#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"
#include "Mixer.h"

// A WALL MAKES A SOURCE QUIETER **AND** DULLER, AND ONLY DOING ONE SOUNDS LIKE A FADER.
//
// Unreal declares both: USoundAttenuation carries an occluded VOLUME and an occluded LOW-PASS
// FREQUENCY, interpolated over a stated time. RAGE's environment groups carry the same pair per
// space. Neither treats occlusion as a gain, because a wall does not attenuate 200 Hz and 4 kHz
// alike -- and a mix that only ducks the volume is the sound of someone turning a knob.
//
// THE ORACLE IS THE FILTER'S OWN MAGNITUDE RESPONSE. The mixer dulls with a one-pole
// y += a*(x - y), whose steady-state gain at angular frequency w is
//
//     |H(w)| = a / sqrt(a^2 + 2*(1 - a)*(1 - cos w))         with a = 1 - exp(-2*pi*fc/rate)
//
// which is derived rather than measured: substitute x = e^{jwn} and solve for the fixed point.
// The case drives a 4 kHz tone through a 400 Hz cutoff, where the response is far from both ends
// of the curve, so an error in `a` or in the recursion shows up rather than hiding at DC.

namespace {

constexpr int kRate = 48000;
constexpr double kToneHz = 4000.0;
constexpr double kCutoffHz = 400.0;
constexpr double kBlockedGain = 0.25;

[[nodiscard]] outshine::Sound Behind() {
  outshine::Sound made;
  made.Id = "tone";
  made.Bus = "master";
  made.FalloffM = 100.0;
  made.Heard.Positional = true;
  made.Heard.By = outshine::Falls::Inverse;
  made.Heard.RefM = 1.0;
  made.Heard.Rolloff = 0.0;
  made.Heard.BlockedGain = kBlockedGain;
  made.Heard.BlockedHz = kCutoffHz;
  outshine::Voice tone;
  tone.Id = "osc";
  tone.Does = outshine::Makes::Oscillator;
  tone.Parameters.push_back(outshine::Setting{"frequency", std::to_string((int)kToneHz)});
  tone.Parameters.push_back(outshine::Setting{"shape", "sine"});
  made.Graph.push_back(tone);
  return made;
}

[[nodiscard]] double Root(const std::vector<float> &stereo) {
  double sum = 0.0;
  size_t counted = 0;
  for (size_t frame = stereo.size() / 4; frame < stereo.size() / 2; ++frame) {
    const double one = (double)stereo[frame * 2 + 1];
    sum += one * one;
    ++counted;
  }
  return counted == 0 ? 0.0 : std::sqrt(sum / (double)counted);
}

[[nodiscard]] double Heard(double blocked) {
  outshine::Audio::Mixer mixer;
  const std::vector<outshine::Bus> buses{outshine::Bus{"master", "", 0.0}};
  const std::vector<outshine::Sound> sounds{Behind()};
  std::string why;
  if (!mixer.Stands(buses, sounds, kRate, why)) { return -1.0; }
  outshine::Audio::Heard where;
  where.Id = "tone";
  where.Standing = true;
  where.AtM[0] = 1.0;
  where.Blocked = blocked;
  outshine::Audio::Listening ear;
  std::vector<float> stereo((size_t)kRate / 4u * 2u, 0.0f);
  if (!mixer.Fills(stereo, std::span<const outshine::Audio::Heard>(&where, 1), ear, why)) {
    return -1.0;
  }
  return Root(stereo);
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double clear = Heard(0.0);
  const double walled = Heard(1.0);
  if (!(clear > 0.0) || !(walled >= 0.0)) {
    Unprepared("the mixer did not stand");
    return Report();
  }

  const double alpha = 1.0 - std::exp(-2.0 * std::numbers::pi * kCutoffHz / (double)kRate);
  const double turn = 2.0 * std::numbers::pi * kToneHz / (double)kRate;
  const double magnitude =
      alpha / std::sqrt(alpha * alpha + 2.0 * (1.0 - alpha) * (1.0 - std::cos(turn)));
  const double owed = clear * kBlockedGain * magnitude;

  std::printf("CLEAR   rms %.6f\n", clear);
  std::printf("WALLED  rms %.6f   the closed form owes %.6f\n", walled, owed);
  std::printf("        gain %.4f x one-pole |H(4 kHz)| %.6f at a 400 Hz cutoff\n", kBlockedGain,
              magnitude);

  CHECK(walled < clear * kBlockedGain,
        "**A WALL DOES BOTH**: the blocked source is quieter than the declared gain alone would "
        "make it, because it is also duller -- and a mix that only ducked the volume would land "
        "exactly ON the gain rather than under it");
  CHECK_NEAR(walled, owed, owed * 0.05, "",
             "and how much duller is the one-pole's own magnitude response, "
             "a / sqrt(a^2 + 2(1-a)(1 - cos w)) -- derived from the recursion rather than "
             "measured off it, so an error in the coefficient cannot hide inside the tolerance");

  Covers("the mixer: an occluded source is attenuated by its DECLARED gain and filtered by its "
         "DECLARED cutoff, and the result is the one-pole's closed-form magnitude at the tone's "
         "own frequency");
  return Report();
}
