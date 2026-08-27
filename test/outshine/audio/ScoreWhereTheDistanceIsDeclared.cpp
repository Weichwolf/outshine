#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "BusGraph.h"
#include "Check.h"
#include "Mixer.h"

// WHERE THE DISTANCE IS DECLARED, AND THAT IT IS DECLARED IN ONE PLACE.
//
// This case exists because the tree carried the distance term TWICE and only one of the two
// sounded (board:1986). A scenario wrote `falloffM` and an emitter, `BusGraph::Build` refused
// any positional sound that omitted `falloffM`, and `Mixer::Fills` read the EMITTER and never
// looked at `falloffM` -- so a factor of a hundred on the declared number changed the mix by
// nothing, measured on apps/demo:
//
//     FalloffM 6, 60, 600     loudest 0.0829, 0.0829, 0.0829
//
// The formulas themselves are proven next door in ScoreWhatDistanceAndSpeedDoToASound. What is
// proven HERE is the other half, which no closed form can state: that the number a scenario
// writes is the number that sounds, and that no second spelling of it stands beside it. So the
// oracle is a RATIO of two mixes of the same graph -- the emitter's own closed form predicts it,
// and a term nothing reads cannot produce it.

namespace {

[[nodiscard]] outshine::Sound Tone(double refM) {
  outshine::Sound made;
  made.Id = "tone";
  made.Bus = "master";
  made.Heard.Positional = true;
  made.Heard.By = outshine::Falls::Inverse;
  made.Heard.RefM = refM;
  made.Heard.Rolloff = 1.0;
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
    const double size = std::fabs((double)one);
    most = size > most ? size : most;
  }
  return most;
}

[[nodiscard]] bool Mixed(double refM, double atM, double &loudest, std::string &why) {
  outshine::Audio::Mixer mixer;
  const std::vector<outshine::Bus> buses{outshine::Bus{.Id = "master", .Into = "", .GainDb = 0.0}};
  const std::vector<outshine::Sound> sounds{Tone(refM)};
  if (!mixer.Stands(buses, sounds, 48000, why)) { return false; }
  outshine::Audio::Heard standing;
  standing.Id = "tone";
  standing.AtM[0] = atM;
  standing.Standing = true;
  outshine::Audio::Listening ear;
  std::vector<float> stereo(4096 * 2, 0.0f);
  if (!mixer.Fills(stereo, std::span<const outshine::Audio::Heard>(&standing, 1), ear, why)) {
    return false;
  }
  loudest = Loudest(stereo);
  return true;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // A DECLARATION THAT NAMES ONLY THE STANDARD'S OWN TERMS STANDS. Web Audio spells the distance
  // as distanceModel, refDistance, maxDistance and rolloffFactor, and a scenario that states them
  // completely was being turned away for omitting a fifth number this tree invented.
  {
    outshine::Audio::BusGraph routing;
    const std::vector<outshine::Bus> buses{outshine::Bus{.Id = "master", .Into = "", .GainDb = 0.0}};
    const std::vector<outshine::Sound> sounds{Tone(4.0)};
    std::string why;
    const bool stood = routing.Build(buses, sounds, why);
    std::printf("  the emitter's terms alone            %s\n", stood ? "STANDS" : why.c_str());
    CHECK(stood,
          "**THE STANDARD'S OWN TERMS ARE A COMPLETE DECLARATION**: distanceModel, refDistance, "
          "maxDistance and rolloffFactor say everything about how a source falls off, and a "
          "refusal that demands a fifth number beside them refuses a correct scenario");
  }

  // A POSITIONAL SOURCE WITHOUT A DISTANCE IS REFUSED, and the refusal names the term that
  // sounds. refDistance of zero makes the inverse model a division by zero, which is why Web
  // Audio requires it above zero and why this is a refusal rather than a default.
  {
    outshine::Audio::BusGraph routing;
    const std::vector<outshine::Bus> buses{outshine::Bus{.Id = "master", .Into = "", .GainDb = 0.0}};
    const std::vector<outshine::Sound> sounds{Tone(0.0)};
    std::string why;
    const bool stood = routing.Build(buses, sounds, why);
    const bool names = why.find("refM") != std::string::npos;
    std::printf("  refM = 0                             %s\n", stood ? "STOOD" : why.c_str());
    CHECK(!stood && names,
          "**A POSITIONAL SOURCE WITH NO DISTANCE IS A STEREO SOURCE WEARING A COSTUME**, and the "
          "refusal must name the term the mix actually reads -- a refusal that names a field "
          "nothing reads is how the two spellings survived beside each other");
  }

  // THE DECLARED NUMBER IS THE NUMBER THAT SOUNDS. Same graph, same place, refDistance doubled.
  // The inverse model is g = ref / (ref + rolloff * (d - ref)); at d = 40 m, ref 5 gives 1/8 and
  // ref 10 gives 1/4, so the ratio is exactly 2. A term nothing reads gives a ratio of exactly 1,
  // which is what this tree measured before the second spelling was deleted.
  {
    double quiet = 0.0, loud = 0.0;
    std::string why;
    if (!Mixed(5.0, 40.0, quiet, why) || !Mixed(10.0, 40.0, loud, why)) {
      Unprepared(why.c_str());
      return Report();
    }
    const double ratio = quiet > 0.0 ? loud / quiet : 0.0;
    std::printf("  refM 5 -> %.4f   refM 10 -> %.4f   ratio %.4f, the closed form owes 2\n",
                quiet, loud, ratio);
    CHECK_NEAR(ratio, 2.0, 0.02, "x",
          "**MOVING THE DECLARED DISTANCE MOVES THE MIX**: at 40 m the inverse model owes "
          "5/(5+35) = 1/8 against 10/(10+30) = 1/4, a ratio of two. A ratio of one says the "
          "declaration is being read from somewhere the mixer does not look");
  }

  Covers("the mixer: the distance a scenario declares is the distance that sounds, declared in "
         "the emitter alone -- a second spelling of it beside the emitter would leave this ratio "
         "at one");
  return Report();
}
