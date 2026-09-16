#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <string>
#include <string_view>

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
  CHECK(source.Buses[0].Reverberates.Declared && source.Buses[0].Reverberates.SecondsRt60 == 0,
        "explicit zero-duration room remains declared");
  CHECK(source.Sounds[0].Heard.By == Scenario::Falls::Exponential &&
            source.Sounds[0].Heard.RefM == 2.5,
        "literal attenuation has specified meaning");
  CHECK(source.Sounds[0].Graph.size() == 2 &&
            source.Sounds[0].Graph[0].Does == Scenario::Makes::Mix &&
            source.Sounds[0].Graph[1].Does == Scenario::Makes::Noise,
        "literal processors retain meaning");
  source.Buses.push_back({.Id = "a&<\"'\t\n\r", .Into = "master", .GainDb = -0.12345678901234567});
  auto &sound = source.Sounds[0];
  sound.Id = "id&\"";
  sound.Uri = "file<&\t\n\r";
  sound.On = "body'";
  sound.Bus = source.Buses[1].Id;
  sound.Streamed = true;
  sound.Loops = true;
  sound.GainDb = -1.2345678901234567;
  sound.SendShare = 0.12345678901234567;
  sound.Heard = {.Positional = true,
                 .By = Scenario::Falls::Linear,
                 .RefM = 1.2345678901234567,
                 .MostM = 245.5,
                 .Rolloff = 0.7,
                 .InnerRad = 0.25,
                 .OuterRad = 0.75,
                 .OuterGain = 0.1,
                 .BlockedGain = 0.2,
                 .BlockedHz = 1350.25};
  constexpr std::array processors{Scenario::Makes::Oscillator,
                                  Scenario::Makes::Noise,
                                  Scenario::Makes::OnePoleLowPass,
                                  Scenario::Makes::Delay,
                                  Scenario::Makes::Gain,
                                  Scenario::Makes::Shaper,
                                  Scenario::Makes::Convolver,
                                  Scenario::Makes::Mix};
  for (const auto processor : processors) {
    sound.Graph.push_back(
        {.Id = "node" + std::to_string(sound.Graph.size()),
         .Does = processor,
         .From = {"osc", "sum", "osc"},
         .Parameters = {{.Name = "a&\t\n", .Value = "'\"<>\r"}, {.Name = "empty", .Value = ""}}});
  }
  constexpr std::array attenuation{
      Scenario::Falls::Linear, Scenario::Falls::Inverse, Scenario::Falls::Exponential};
  for (const auto law : attenuation) {
    sound.Heard.By = law;
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
      CHECK(a.Id == b.Id && a.Into == b.Into && a.GainDb == b.GainDb,
            "bus routing and gain survive");
      CHECK(a.Reverberates.Declared == b.Reverberates.Declared &&
                a.Reverberates.SecondsRt60 == b.Reverberates.SecondsRt60 &&
                a.Reverberates.Damping == b.Reverberates.Damping &&
                a.Reverberates.WetShare == b.Reverberates.WetShare,
            "room parameters survive");
    }
    const auto &b = copy.Sounds[0];
    CHECK(sound.Id == b.Id && sound.Uri == b.Uri && sound.On == b.On && sound.Bus == b.Bus &&
              sound.Streamed == b.Streamed && sound.Loops == b.Loops && sound.GainDb == b.GainDb &&
              sound.SendShare == b.SendShare,
          "sound metadata retains exact values");
    const auto &a = sound.Heard;
    const auto &h = b.Heard;
    CHECK(a.Positional == h.Positional && a.By == h.By && a.RefM == h.RefM && a.MostM == h.MostM &&
              a.Rolloff == h.Rolloff && a.InnerRad == h.InnerRad && a.OuterRad == h.OuterRad &&
              a.OuterGain == h.OuterGain && a.BlockedGain == h.BlockedGain &&
              a.BlockedHz == h.BlockedHz,
          "all emitter fields retain exact values");
    CHECK(sound.Graph.size() == b.Graph.size(), "all processors survive");
    if (sound.Graph.size() != b.Graph.size()) { continue; }
    for (size_t i = 0; i < sound.Graph.size(); ++i) {
      const auto &v = sound.Graph[i];
      const auto &w = b.Graph[i];
      CHECK(v.Id == w.Id && v.Does == w.Does && v.From == w.From,
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
  sound.Heard.By = static_cast<Scenario::Falls>(255);
  CHECK(!WriteScenario(source), "invalid attenuation is not replaced by a default");
  sound.Heard.By = Scenario::Falls::Inverse;
  sound.Graph[0].Does = static_cast<Scenario::Makes>(255);
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
            migrated.Sounds[0].Graph[0].Does == Scenario::Makes::OnePoleLowPass,
        "legacy token has the truthful native processor");
  const auto canonical = WriteScenario(migrated);
  CHECK(canonical && canonical->find("does=\"onePoleLowPass\"") != std::string::npos &&
            canonical->find("biquad") == std::string::npos,
        "writer emits only the truthful filter token");
  for (const std::string_view attributes : {"does=\"unknown\"", "falls=\"unknown\""}) {
    const std::string malformed = "<scenario><audio><sound id=\"bad\" " + std::string(attributes) +
                                  "><voice id=\"tone\"/></sound></audio></scenario>";
    CHECK(!ReadScenario(malformed.data(), malformed.size(), migrated, error),
          "unknown audio enum rejects the whole declaration");
    CHECK(migrated.Sounds.size() == 1 && migrated.Sounds[0].Graph.size() == 1 &&
              migrated.Sounds[0].Graph[0].Does == Scenario::Makes::OnePoleLowPass,
          "unknown audio enum preserves the prior declaration");
  }
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<audio>") == std::string::npos, "empty audio remains absent");
  return Report();
}
