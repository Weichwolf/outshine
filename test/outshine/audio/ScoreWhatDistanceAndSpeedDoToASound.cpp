#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Mixer.h"

// WHAT THE WORLD DOES TO A SOUND, AGAINST THE CLOSED FORMS THAT DEFINE IT.
//
// The Web Audio API states three distance models and the classical Doppler ratio states the
// fourth, so none of the numbers below is ours: each is a formula written down by someone else
// and evaluated by hand here. That is TRUTH grade -- the case fails because the code is wrong,
// never because our oracle drifted.
//
//   inverse      g = ref / (ref + rolloff * (d - ref))          at d = 2*ref, rolloff 1  -> 1/2
//   exponential  g = (d / ref)^(-rolloff)                       at d = 2*ref, rolloff 2  -> 1/4
//   linear       g = 1 - rolloff * (d - ref) / (max - ref)      halfway between            -> 1/2
//   Doppler      f' / f = (c + v_ear.r) / (c + v_source.r)   with r pointing from EAR to SOURCE
//
// The sign convention is the one that trips everybody, so it is stated rather than assumed: r
// points from the EAR to the SOURCE, so the ear's component along r is its speed TOWARD the
// source and the source's is its speed AWAY. A source closing on the ear therefore has a NEGATIVE
// component, which shrinks the denominator and raises the pitch -- the ambulance coming at you.
// The case drives it at exactly c/10 so the answer is 10/9 and not a decimal nobody can check by
// eye, and it caught this exact error on the first run: 401 Hz, which is 9/10 rather than 10/9.

namespace {

[[nodiscard]] outshine::Sound Tone(const std::string &id, outshine::Falls by, double rolloff) {
  outshine::Sound made;
  made.Id = id;
  made.Bus = "master";
  made.Heard.Positional = true;
  made.Heard.By = by;
  made.Heard.RefM = 10.0;
  made.Heard.MostM = 110.0;
  made.Heard.Rolloff = rolloff;
  outshine::Voice one;
  one.Id = "osc";
  one.Does = outshine::Makes::Oscillator;
  one.Parameters.push_back(outshine::Setting{"frequency", "441"});
  one.Parameters.push_back(outshine::Setting{"shape", "sine"});
  made.Graph.push_back(one);
  return made;
}

[[nodiscard]] double Loudest(const std::vector<float> &stereo) {
  double most = 0.0;
  for (const float one : stereo) {
    most = std::fabs((double)one) > most ? std::fabs((double)one) : most;
  }
  return most;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<outshine::Bus> buses{outshine::Bus{.Id = "master", .Into = "", .GainDb = 0.0}};
  const int rate = 48000;

  for (const auto &[named, by, rolloff, atM, owed] :
       std::vector<std::tuple<const char *, outshine::Falls, double, double, double>>{
           {"inverse", outshine::Falls::Inverse, 1.0, 20.0, 0.5},
           {"exponential", outshine::Falls::Exponential, 2.0, 20.0, 0.25},
           {"linear", outshine::Falls::Linear, 1.0, 60.0, 0.5}}) {
    outshine::Audio::Mixer mixer;
    const std::vector<outshine::Sound> sounds{Tone("tone", by, rolloff)};
    std::string why;
    if (!mixer.Stands(buses, sounds, rate, why)) {
      Unprepared(why.c_str());
      return Report();
    }
    outshine::Audio::Heard where;
    where.Id = "tone";
    where.Standing = true;
    where.AtM[0] = atM;
    outshine::Audio::Listening ear;
    std::vector<float> stereo(2048 * 2, 0.0f);
    if (!mixer.Fills(stereo, std::span<const outshine::Audio::Heard>(&where, 1), ear, why)) {
      Unprepared(why.c_str());
      return Report();
    }
    const double loudest = Loudest(stereo);
    std::printf(
        "%-12s at %5.1f m   peak %.4f   the standard owes %.4f\n", named, atM, loudest, owed);
    CHECK_NEAR(
        loudest,
        owed,
        0.02,
        "",
        ("the " + std::string(named) + " distance model is the Web Audio formula and nothing else")
            .c_str());
  }

  outshine::Audio::Mixer mixer;
  const std::vector<outshine::Sound> sounds{Tone("tone", outshine::Falls::Inverse, 0.0)};
  std::string why;
  if (!mixer.Stands(buses, sounds, rate, why)) {
    Unprepared(why.c_str());
    return Report();
  }
  const double speedMs = mixer.SpeedOfSoundMs();
  outshine::Audio::Heard closing;
  closing.Id = "tone";
  closing.Standing = true;
  closing.AtM[0] = 10.0;
  closing.VelocityMs[0] = -speedMs / 10.0;

  outshine::Audio::Listening ear;
  std::vector<float> stereo(rate * 2, 0.0f);
  if (!mixer.Fills(stereo, std::span<const outshine::Audio::Heard>(&closing, 1), ear, why)) {
    Unprepared(why.c_str());
    return Report();
  }
  size_t crossings = 0;
  for (size_t frame = 1; frame < stereo.size() / 2; ++frame) {
    if (stereo[(frame - 1) * 2 + 1] <= 0.0f && stereo[frame * 2 + 1] > 0.0f) { ++crossings; }
  }
  const double heardHz = (double)crossings;
  const double owedHz = 441.0 * 10.0 / 9.0;
  std::printf("DOPPLER  source closing at c/10   heard %.1f Hz   the closed form owes %.1f Hz\n",
              heardHz,
              owedHz);
  CHECK_NEAR(
      heardHz,
      owedHz,
      2.0,
      "Hz",
      "**A SOURCE CLOSING AT A TENTH OF THE SPEED OF SOUND IS HEARD TEN NINTHS HIGH**: the "
      "classical ratio is (c + v_ear.r) / (c + v_source.r) with r from ear to source, and "
      "441 Hz becomes 490 Hz. A sign error either way lands on 9/10 and 397 Hz, which is why "
      "the case drives it at exactly c/10");

  Covers("the mixer: the three Web Audio distance models and the classical Doppler ratio, each "
         "against the closed form that defines it rather than against a number this tree chose");
  return Report();
}
