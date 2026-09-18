#ifndef OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#define OUTSHINE_SCENARIO_AUDIOSPELLINGS_H
#include "Spelling.h"
#include <audio/AudioScene.h>
#include <optional>
#include <string_view>

namespace outshine::AudioFormat {
inline constexpr Spellings<Audio::AttenuationModel, 3> kAttenuationModels = {
    {{"linear", Audio::AttenuationModel::Linear},
     {"inverse", Audio::AttenuationModel::Inverse},
     {"exponential", Audio::AttenuationModel::Exponential}}};

inline constexpr Spellings<Audio::ProcessorKind, 8> kProcessorKinds = {
    {{"oscillator", Audio::ProcessorKind::Oscillator},
     {"noise", Audio::ProcessorKind::Noise},
     {"onePoleLowPass", Audio::ProcessorKind::OnePoleLowPass},
     {"delay", Audio::ProcessorKind::Delay},
     {"gain", Audio::ProcessorKind::Gain},
     {"shaper", Audio::ProcessorKind::Shaper},
     {"convolver", Audio::ProcessorKind::Convolver},
     {"mix", Audio::ProcessorKind::Mix}}};

static_assert(EverySpellingStandsOnce(kAttenuationModels) &&
                  EverySpellingStandsOnce(kProcessorKinds),
              "a spelling table that carries a blank or a repeat resolves a declaration by "
              "whichever row it reaches first");

[[nodiscard]] constexpr std::optional<Audio::AttenuationModel>
AttenuationModelNamed(std::string_view spelling) {
  for (const auto &[text, value] : kAttenuationModels) {
    if (text == spelling) { return value; }
  }
  return std::nullopt;
}

[[nodiscard]] constexpr std::optional<Audio::ProcessorKind>
ProcessorKindNamed(std::string_view spelling) {
  if (spelling == "biquad") { return Audio::ProcessorKind::OnePoleLowPass; }
  for (const auto &[text, value] : kProcessorKinds) {
    if (text == spelling) { return value; }
  }
  return std::nullopt;
}

}
#endif
