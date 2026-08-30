#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Mixer.h"

// A ROOM HOLDS A SOUND FOR THE TIME IT DECLARES, AND RT60 IS THE MEASUREMENT THAT SAYS SO.
//
// Unreal puts reverb on a SUBMIX and the space that declares it is an AAudioVolume; RAGE puts it
// on an environment group per space. Both make it a property of the ROOM with a SEND from the
// source, never a computation per source -- and Unreal keeps ray-traced early reflections in a
// plugin (Steam Audio, Resonance) rather than the base engine, which is why this item stops at
// the send and the room.
//
// THE ORACLE IS THE COMB'S OWN DECAY. A feedback comb with delay D and feedback g loses
// 20*log10(g) dB every D seconds, so it reaches -60 dB at RT60 = -3*D / log10(g) -- and inverting
// that is how the mixer chooses g from a declared RT60: g = 10^(-3D/RT60). The case declares a
// room, strikes it once, and measures how long the tail takes to fall 60 dB. If the inversion
// were wrong the tail would be visibly short or endless, and neither hides in a tolerance.

namespace {

constexpr int kRate = 24000;
constexpr double kRt60 = 1.5;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Bus master;
  master.Id = "master";
  master.GainDb = 0.0;
  master.Reverberates.Declared = true;
  master.Reverberates.SecondsRt60 = kRt60;
  master.Reverberates.Damping = 0.0;
  master.Reverberates.WetShare = 1.0;

  outshine::Sound struck;
  struck.Id = "strike";
  struck.Bus = "master";
  struck.SendShare = 1.0;
  outshine::Voice tone;
  tone.Id = "osc";
  tone.Does = outshine::Makes::Oscillator;
  tone.Parameters.push_back(outshine::Setting{"frequency", "220"});
  struck.Graph.push_back(tone);

  outshine::Audio::Mixer mixer;
  std::string why;
  const std::vector<outshine::Bus> buses{master};
  const std::vector<outshine::Sound> sounds{struck};
  if (!mixer.Stands(buses, sounds, kRate, why)) {
    Unprepared(why.c_str());
    return Report();
  }

  outshine::Audio::Heard where;
  where.Id = "strike";
  where.Standing = true;

  std::vector<float> stereo(2048u * 2u, 0.0f);
  double struckPeak = 0.0;
  for (int block = 0; block < 4; ++block) {
    if (!mixer.Fills(stereo,
                     std::span<const outshine::Audio::Heard>(&where, 1),
                     outshine::Audio::Listening{},
                     why)) {
      Unprepared(why.c_str());
      return Report();
    }
    for (const float one : stereo) {
      struckPeak = std::fabs((double)one) > struckPeak ? std::fabs((double)one) : struckPeak;
    }
  }

  where.Standing = false;

  double heldS = 0.0;
  const double owedShare = std::pow(10.0, -60.0 / 20.0);
  for (int block = 0; block < 200; ++block) {
    if (!mixer.Fills(stereo,
                     std::span<const outshine::Audio::Heard>(&where, 1),
                     outshine::Audio::Listening{},
                     why)) {
      break;
    }
    double loudest = 0.0;
    for (const float one : stereo) {
      loudest = std::fabs((double)one) > loudest ? std::fabs((double)one) : loudest;
    }
    heldS += (double)(stereo.size() / 2) / (double)kRate;
    if (loudest < struckPeak * owedShare) { break; }
  }

  std::printf("ROOM  declares RT60 %.2f s   the tail fell 60 dB in %.2f s\n", kRt60, heldS);
  std::printf("      g = 10^(-3D/RT60) per comb, struck peak %.4f\n", struckPeak);

  CHECK(struckPeak > 0.0, "the room is struck at all -- a send of one into a declared room");
  CHECK_NEAR(heldS,
             kRt60,
             kRt60 * 0.35,
             "s",
             "**A ROOM HOLDS A SOUND FOR THE TIME IT DECLARES**: RT60 = -3*D / log10(g) is the "
             "comb's own decay, and the mixer inverts it to choose g from the declared time. A "
             "wrong inversion gives a tail that is visibly short or endless, and the tolerance is "
             "wide because four combs of different lengths sum to a decay that is only "
             "approximately one exponential -- which is what a Schroeder reverb IS");

  Covers("the mixer: a bus declares a room and a source sends into it, and the tail falls 60 dB "
         "in the time the room declared -- the comb decay RT60 = -3D/log10(g), inverted");
  return Report();
}
