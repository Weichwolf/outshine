#ifndef OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#define OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#include "Spelling.h"
#include <scenario/Scenario.h>

namespace outshine::AudioFormat {
inline constexpr Spellings<Scenario::Falls, 3> kFalls = {
    {{"linear", Scenario::Falls::Linear},
     {"inverse", Scenario::Falls::Inverse},
     {"exponential", Scenario::Falls::Exponential}}};

inline constexpr Spellings<Scenario::Makes, 8> kMakes = {
    {{"oscillator", Scenario::Makes::Oscillator},
     {"noise", Scenario::Makes::Noise},
     {"biquad", Scenario::Makes::Biquad},
     {"delay", Scenario::Makes::Delay},
     {"gain", Scenario::Makes::Gain},
     {"shaper", Scenario::Makes::Shaper},
     {"convolver", Scenario::Makes::Convolver},
     {"mix", Scenario::Makes::Mix}}};

static_assert(EverySpellingStandsOnce(kFalls) && EverySpellingStandsOnce(kMakes),
              "a spelling table that carries a blank or a repeat resolves a declaration by "
              "whichever row it reaches first");

}
#endif
