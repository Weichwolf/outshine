#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace {
constexpr double kLiteralReferenceM = 2.5;
constexpr double kPreciseFraction = 0.12345678901234567;
constexpr double kPreciseValue = 1.2345678901234567;
constexpr double kMaximumDistanceM = 245.5;
constexpr double kRolloff = 0.7;
constexpr double kInnerConeRad = 0.25;
constexpr double kOuterConeRad = 0.75;
constexpr double kOuterConeGain = 0.1;
constexpr double kObstructedGain = 0.2;
constexpr double kObstructedCutoffHz = 1350.25;
constexpr unsigned kUnknownEnum = 255;

void FillRoundTripAudio(outshine::Scenario::Document &source) {
  using namespace outshine;
  using namespace outshine::Test;
  source.Buses.push_back(
      {.Id = "a&<\"'\t\n\r", .Output = "master", .GainDb = -kPreciseFraction, .Reverberation = {}});
  auto &sound = source.Sounds[0];
  sound.Id = "id&\"";
  sound.Uri = "file<&\t\n\r";
  sound.Body = "body'";
  sound.Bus = source.Buses[1].Id;
  sound.Streamed = true;
  sound.Loops = true;
  sound.GainDb = -kPreciseValue;
  sound.ReverbSend = kPreciseFraction;
  sound.Spatial = {.Positional = true,
                   .Attenuation = Audio::AttenuationModel::Linear,
                   .ReferenceDistanceM = kPreciseValue,
                   .MaximumDistanceM = kMaximumDistanceM,
                   .Rolloff = kRolloff,
                   .InnerConeRad = kInnerConeRad,
                   .OuterConeRad = kOuterConeRad,
                   .OuterConeGain = kOuterConeGain,
                   .ObstructedGain = kObstructedGain,
                   .ObstructedCutoffHz = kObstructedCutoffHz};
  constexpr std::array processors{Audio::ProcessorKind::Oscillator,
                                  Audio::ProcessorKind::Noise,
                                  Audio::ProcessorKind::OnePoleLowPass,
                                  Audio::ProcessorKind::Delay,
                                  Audio::ProcessorKind::Gain,
                                  Audio::ProcessorKind::Shaper,
                                  Audio::ProcessorKind::Convolver,
                                  Audio::ProcessorKind::Mix};
  for (const auto processor : processors) {
    sound.Graph.push_back(
        {.Id = "node" + std::to_string(sound.Graph.size()),
         .Processor = processor,
         .Inputs = {"osc", "sum", "osc"},
         .Parameters = {{.Name = "a&\t\n", .Value = "'\"<>\r"}, {.Name = "empty", .Value = ""}}});
  }
}

void CheckAudioRoundTrips(outshine::Scenario::Document &source) {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  auto &sound = source.Sounds[0];
  constexpr std::array attenuation{Audio::AttenuationModel::Linear,
                                   Audio::AttenuationModel::Inverse,
                                   Audio::AttenuationModel::Exponential};
  for (const auto law : attenuation) {
    sound.Spatial.Attenuation = law;
    const auto written = WriteScenario(source);
    CHECK(written.has_value(), "audio export succeeds");
    if (!written) { continue; }
    Scenario::Document copy;
    CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
    CHECK(copy.Buses.size() == 2 && copy.Sounds.size() == 1, "audio catalogs survive");
    if (copy.Buses.size() != 2 || copy.Sounds.size() != 1) { continue; }
    for (size_t i = 0; i < source.Buses.size(); ++i) {
      const auto &a = source.Buses[i];
      const auto &b = copy.Buses[i];
      CHECK(a.Id == b.Id && a.Output == b.Output && a.GainDb == b.GainDb,
            "bus routing and gain survive");
      CHECK(a.Reverberation.Enabled == b.Reverberation.Enabled &&
                a.Reverberation.DecayTimeS == b.Reverberation.DecayTimeS &&
                a.Reverberation.Damping == b.Reverberation.Damping &&
                a.Reverberation.WetGain == b.Reverberation.WetGain,
            "room parameters survive");
    }
    const auto &b = copy.Sounds[0];
    CHECK(sound.Id == b.Id && sound.Uri == b.Uri && sound.Body == b.Body && sound.Bus == b.Bus &&
              sound.Streamed == b.Streamed && sound.Loops == b.Loops && sound.GainDb == b.GainDb &&
              sound.ReverbSend == b.ReverbSend,
          "sound metadata retains exact values");
    const auto &a = sound.Spatial;
    const auto &h = b.Spatial;
    CHECK(a.Positional == h.Positional && a.Attenuation == h.Attenuation &&
              a.ReferenceDistanceM == h.ReferenceDistanceM &&
              a.MaximumDistanceM == h.MaximumDistanceM && a.Rolloff == h.Rolloff &&
              a.InnerConeRad == h.InnerConeRad && a.OuterConeRad == h.OuterConeRad &&
              a.OuterConeGain == h.OuterConeGain && a.ObstructedGain == h.ObstructedGain &&
              a.ObstructedCutoffHz == h.ObstructedCutoffHz,
          "all emitter fields retain exact values");
    CHECK(sound.Graph.size() == b.Graph.size(), "all processors survive");
    if (sound.Graph.size() != b.Graph.size()) { continue; }
    for (size_t i = 0; i < sound.Graph.size(); ++i) {
      const auto &v = sound.Graph[i];
      const auto &w = b.Graph[i];
      CHECK(v.Id == w.Id && v.Processor == w.Processor && v.Inputs == w.Inputs,
            "processor order and repeated forward inputs survive");
      CHECK(v.Parameters.size() == w.Parameters.size(), "parameter count survives");
      if (v.Parameters.size() != w.Parameters.size()) { continue; }
      for (size_t j = 0; j < v.Parameters.size(); ++j) {
        CHECK(v.Parameters[j].Name == w.Parameters[j].Name &&
                  v.Parameters[j].Value == w.Parameters[j].Value,
              "escaped and empty settings survive");
      }
    }
  }
}

void CheckRejectedAudioSpellings(outshine::Scenario::Document &source) {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  auto &sound = source.Sounds[0];
  sound.Spatial.Attenuation = static_cast<Audio::AttenuationModel>(kUnknownEnum);
  CHECK(!WriteScenario(source), "invalid attenuation is not replaced by a default");
  sound.Spatial.Attenuation = Audio::AttenuationModel::Inverse;
  sound.Graph[0].Processor = static_cast<Audio::ProcessorKind>(kUnknownEnum);
  CHECK(!WriteScenario(source), "invalid processor is not replaced by a default");
  constexpr std::string_view ignoredBusVoice =
      R"(<scenario><audio><bus id="master"><voice id="lost"/></bus></audio></scenario>)";
  Scenario::Document rejected;
  CHECK(!ReadScenario(ignoredBusVoice.data(), ignoredBusVoice.size(), rejected, error),
        "unsupported bus processor is rejected instead of ignored");
  constexpr std::string_view legacy =
      R"(<scenario><audio><sound id="legacy"><voice id="filter" does="biquad"/></sound></audio></scenario>)";
  Scenario::Document migrated;
  CHECK(ReadScenario(legacy.data(), legacy.size(), migrated, error),
        "legacy filter token remains readable");
  CHECK(migrated.Sounds.size() == 1 && migrated.Sounds[0].Graph.size() == 1 &&
            migrated.Sounds[0].Graph[0].Processor == Audio::ProcessorKind::OnePoleLowPass,
        "legacy token has the truthful native processor");
  const auto canonical = WriteScenario(migrated);
  CHECK(canonical && canonical->contains("does=\"onePoleLowPass\"") &&
            !canonical->contains("biquad"),
        "writer emits only the truthful filter token");
  for (const std::string_view attributes : {"does=\"unknown\"", "falls=\"unknown\""}) {
    const std::string malformed = "<scenario><audio><sound id=\"bad\" " + std::string(attributes) +
                                  "><voice id=\"tone\"/></sound></audio></scenario>";
    CHECK(!ReadScenario(malformed.data(), malformed.size(), migrated, error),
          "unknown audio enum rejects the whole declaration");
    CHECK(migrated.Sounds.size() == 1 && migrated.Sounds[0].Graph.size() == 1 &&
              migrated.Sounds[0].Graph[0].Processor == Audio::ProcessorKind::OnePoleLowPass,
          "unknown audio enum preserves the prior declaration");
  }
  const auto empty = WriteScenario({});
  CHECK(empty && !empty->contains("<audio>"), "empty audio remains absent");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view fixture = R"(<scenario><audio>
    <bus id="master" gainDb="-3.5"><room secondsRt60="0" damping="0.25" wetShare="0.75"/></bus>
    <sound id="engine" falls="exponential" refM="2.5" positional="yes">
      <voice id="sum" does="mix"><from id="osc"/><from id="osc"/></voice>
      <voice id="osc" does="noise"><set name="color" value="pink"/></voice>
    </sound></audio></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(fixture.data(), fixture.size(), source, error), error.c_str());
  CHECK(source.Buses.size() == 1 && source.Sounds.size() == 1, "literal XML loads audio");
  if (source.Buses.size() != 1 || source.Sounds.size() != 1) { return Report(); }
  CHECK(source.Buses[0].Reverberation.Enabled && source.Buses[0].Reverberation.DecayTimeS == 0,
        "explicit zero-duration room remains declared");
  CHECK(source.Sounds[0].Spatial.Attenuation == Audio::AttenuationModel::Exponential &&
            source.Sounds[0].Spatial.ReferenceDistanceM == kLiteralReferenceM,
        "literal attenuation has specified meaning");
  CHECK(source.Sounds[0].Graph.size() == 2 &&
            source.Sounds[0].Graph[0].Processor == Audio::ProcessorKind::Mix &&
            source.Sounds[0].Graph[1].Processor == Audio::ProcessorKind::Noise,
        "literal processors retain meaning");
  FillRoundTripAudio(source);
  CheckAudioRoundTrips(source);
  CheckRejectedAudioSpellings(source);
  return Report();
}
