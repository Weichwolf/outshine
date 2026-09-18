#ifndef OUTSHINE_AUDIO_AUDIOSCENE_H
#define OUTSHINE_AUDIO_AUDIOSCENE_H

#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Audio {

/// Distance attenuation law evaluated for positional sound sources.
enum class AttenuationModel : uint8_t {
  Linear,     ///< Linear attenuation between the reference and maximum distance.
  Inverse,    ///< Reciprocal attenuation from the reference distance.
  Exponential ///< Exponential attenuation using the configured rolloff coefficient.
};

/// Owned spatialization parameters copied into a prepared audio scene.
struct SpatialSource {
  bool Positional = false; ///< Enable distance, panning, Doppler and obstruction processing.
  AttenuationModel Attenuation = AttenuationModel::Inverse; ///< Distance gain law.
  double ReferenceDistanceM = 1.0; ///< Positive distance retaining the source reference gain.
  double MaximumDistanceM = 0.0;   ///< Linear-law endpoint; nonpositive selects twice reference.
  double Rolloff = 1.0;            ///< Finite nonnegative attenuation coefficient.
  double InnerConeRad = 0.0;  ///< Retained inner cone angle; directional cones are not applied yet.
  double OuterConeRad = 0.0;  ///< Retained outer cone angle; directional cones are not applied yet.
  double OuterConeGain = 0.0; ///< Retained outer cone gain; directional cones are not applied yet.
  double ObstructedGain = 1.0;     ///< Gain in [0,1] at full obstruction.
  double ObstructedCutoffHz = 0.0; ///< Low-pass cutoff at obstruction; zero disables filtering.
};

/// DSP operation performed by one signal node.
enum class ProcessorKind : uint8_t {
  Oscillator,     ///< Periodic waveform generator.
  Noise,          ///< Noise generator.
  OnePoleLowPass, ///< First-order low-pass filter.
  Delay,          ///< Prepared delay line with optional internal feedback.
  Gain,           ///< Scale summed input samples.
  Shaper,         ///< Reserved nonlinear processor; setup currently rejects it.
  Convolver,      ///< Reserved convolution processor; setup currently rejects it.
  Mix             ///< Sum input nodes.
};

/// Owned name/value parameter interpreted by its DSP processor.
struct SignalParameter {
  std::string Name;  ///< Exact case-sensitive parameter name.
  std::string Value; ///< Owned textual value parsed during audio preparation.
};

/// Owned declarative node in an acyclic source signal graph.
struct SignalNode {
  std::string Id; ///< Nonempty identifier unique within the containing source graph.
  ProcessorKind Processor = ProcessorKind::Oscillator; ///< Operation performed by this node.
  std::vector<std::string> Inputs;                     ///< Input node IDs in summation order.
  std::vector<SignalParameter> Parameters;             ///< Owned processor parameters.
};

/// Owned sound-source configuration without live playback or DSP state.
struct SoundSource {
  std::string Id;                ///< Nonempty source identifier unique in the prepared catalogue.
  std::string Uri;               ///< Retained file reference; decoding is not implemented yet.
  std::vector<SignalNode> Graph; ///< Acyclic graph whose last node is the mono output.
  bool Streamed = false;   ///< Retained client-buffer intent; submission is not implemented yet.
  std::string Body;        ///< Body name supplying the source transform; empty leaves it unbound.
  std::string Bus;         ///< Destination bus ID; empty selects the unique master.
  SpatialSource Spatial;   ///< Spatialization and obstruction parameters.
  bool Loops = false;      ///< Retained playback intent for future source scheduling.
  double GainDb = 0.0;     ///< Finite source gain in decibels.
  double ReverbSend = 0.0; ///< Finite nonnegative send to the shared reverb.
};

/// Owned shared reverberation configuration.
struct Reverb {
  bool Enabled = false;    ///< Enable validation and use of the remaining settings.
  double DecayTimeS = 0.0; ///< Finite nonnegative time to decay by 60 dB.
  double Damping = 0.5;    ///< Finite high-frequency damping coefficient in [0,1].
  double WetGain = 0.0;    ///< Finite wet-output gain in [0,1].
};

/// Owned audio-routing node copied into the prepared mixer graph.
struct MixBus {
  std::string Id;       ///< Nonempty ID unique in the bus graph.
  std::string Output;   ///< Parent bus ID; empty identifies the unique master.
  double GainDb = 0.0;  ///< Finite bus gain in decibels.
  Reverb Reverberation; ///< Shared reverb declaration; only one active instance is supported.
};

}

#endif
