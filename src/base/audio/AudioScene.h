#ifndef OUTSHINE_BASE_AUDIO_AUDIOSCENE_H
#define OUTSHINE_BASE_AUDIO_AUDIOSCENE_H

#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Audio {

enum class AttenuationModel : uint8_t { Linear, Inverse, Exponential };

struct SpatialSource {
  bool Positional = false;
  AttenuationModel Attenuation = AttenuationModel::Inverse;
  double ReferenceDistanceM = 1.0;
  double MaximumDistanceM = 0.0;
  double Rolloff = 1.0;
  double InnerConeRad = 0.0;
  double OuterConeRad = 0.0;
  double OuterConeGain = 0.0;
  double ObstructedGain = 1.0;
  double ObstructedCutoffHz = 0.0;
};

enum class ProcessorKind : uint8_t {
  Oscillator,
  Noise,
  OnePoleLowPass,
  Delay,
  Gain,
  Shaper,
  Convolver,
  Mix
};

struct SignalParameter {
  std::string Name;
  std::string Value;
};

struct SignalNode {
  std::string Id;
  ProcessorKind Processor = ProcessorKind::Oscillator;
  std::vector<std::string> Inputs;
  std::vector<SignalParameter> Parameters;
};

struct SoundSource {
  std::string Id;
  std::string Uri;
  std::vector<SignalNode> Graph;
  bool Streamed = false;
  std::string Body;
  std::string Bus;
  SpatialSource Spatial;
  bool Loops = false;
  double GainDb = 0.0;
  double ReverbSend = 0.0;
};

struct Reverb {
  bool Enabled = false;
  double DecayTimeS = 0.0;
  double Damping = 0.5;
  double WetGain = 0.0;
};

struct MixBus {
  std::string Id;
  std::string Output;
  double GainDb = 0.0;
  Reverb Reverberation;
};

}

#endif
