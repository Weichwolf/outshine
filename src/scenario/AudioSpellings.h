#ifndef OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#define OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#include "Spelling.h"
#include <scenario/Scenario.h>
#include <optional>
#include <string_view>

namespace outshine::AudioFormat {
inline constexpr Spellings<Scenario::Falls, 3> kFalls = {
    {{"linear", Scenario::Falls::Linear},
     {"inverse", Scenario::Falls::Inverse},
     {"exponential", Scenario::Falls::Exponential}}};

inline constexpr Spellings<Scenario::Makes, 8> kMakes = {
    {{"oscillator", Scenario::Makes::Oscillator},
     {"noise", Scenario::Makes::Noise},
     {"onePoleLowPass", Scenario::Makes::OnePoleLowPass},
     {"delay", Scenario::Makes::Delay},
     {"gain", Scenario::Makes::Gain},
     {"shaper", Scenario::Makes::Shaper},
     {"convolver", Scenario::Makes::Convolver},
     {"mix", Scenario::Makes::Mix}}};

static_assert(EverySpellingStandsOnce(kFalls) && EverySpellingStandsOnce(kMakes),
              "a spelling table that carries a blank or a repeat resolves a declaration by "
              "whichever row it reaches first");

[[nodiscard]] constexpr std::optional<Scenario::Falls> FallNamed(std::string_view spelling) {
  for (const auto &[text, value] : kFalls) {
    if (text == spelling) { return value; }
  }
  return std::nullopt;
}

[[nodiscard]] constexpr std::optional<Scenario::Makes> MakeNamed(std::string_view spelling) {
  if (spelling == "biquad") { return Scenario::Makes::OnePoleLowPass; }
  for (const auto &[text, value] : kMakes) {
    if (text == spelling) { return value; }
  }
  return std::nullopt;
}

}
#endif
