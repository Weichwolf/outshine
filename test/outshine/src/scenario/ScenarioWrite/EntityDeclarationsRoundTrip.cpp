#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <string_view>

void CheckSettings(std::span<const outshine::Scenario::Setting> a,
                   std::span<const outshine::Scenario::Setting> b) {
  using namespace outshine::Test;
  CHECK(a.size() == b.size(), "attribute count survives");
  if (a.size() != b.size()) { return; }
  for (size_t i = 0; i < a.size(); ++i) {
    CHECK(a[i].Name == b[i].Name && a[i].Value == b[i].Value,
          "ordered duplicate attributes and escaped values survive");
  }
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario>
    <kinds><kind name="base"><may do="walk"/><has name="health" value="100"/></kind>
      <kind name="actor" inherits="base" asset="a&amp;b"><mind tier="local" hz="4.5"/>
        <has name="health" value="80"/><has name="health" value="75"/></kind></kinds>
    <instances><instance of="actor" id="parent"><holds what="child"/></instance>
      <instance of="base" id="child" in="parent" x="1.25" scale="2" scaleY="-3">
        <has name="health" value="42"/></instance></instances></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), error.c_str());
  if (source.Kinds.size() != 2 || source.Instances.size() != 2) {
    CHECK(false, "literal fixture loads both declaration catalogs");
    return Report();
  }
  CHECK(source.Kinds[1].Inherits == "base" && source.Kinds[1].Minds[0].Hz == 4.5,
        "literal prefab has explicit inheritance and timing metadata");
  CHECK(source.Instances[1].Stands.ScaleXyz[0] == 2 && source.Instances[1].Stands.ScaleXyz[1] == -3,
        "literal instance retains legacy scale and per-axis override");
  source.Kinds[1].Capabilities = {"a&<\"'\t\n\r", "walk", "walk"};
  auto &mind = source.Kinds[1].Minds[0];
  mind.Uses = "p&\"";
  mind.Programme = "code\n";
  mind.Prompt = "x\ty\rz";
  mind.Model = "model'";
  mind.Meanwhile = "fallback<";
  mind.EverySeconds = 0.12345678901234567;
  mind.StepBudget = std::numeric_limits<long long>::max();
  mind.TokenBudget = std::numeric_limits<int>::max();
  mind.LatencyBudgetMs = 12.345678901234567;
  mind.Temperature = 0.75;
  mind.Seed = std::numeric_limits<long long>::min();
  source.Kinds[1].Minds.push_back({});
  auto &pose = source.Instances[1].Stands;
  pose.AtM = {std::nextafter(6000000.0, 7000000.0), -2.5, 3.75};
  pose.Facing = {.X = 0.1, .Y = 0.2, .Z = 0.3, .W = 0.4};
  pose.GlobeAnchor = true;
  pose.SamplesHeight = true;
  pose.Geodetic = {.LongitudeDeg = 11.25, .LatitudeDeg = 47.5, .HeightM = 123.45678901234567};
  pose.BearingDeg = 90;
  pose.PitchDeg = -15;
  source.Instances[0].Holds = {"child", "other&\"", "child"};
  source.Instances[1].Attributes.push_back({"health", ""});
  const auto written = WriteScenario(source);
  CHECK(written.has_value(), "entity catalogs export");
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
  CHECK(copy.Kinds.size() == 2 && copy.Instances.size() == 2, "both catalogs survive export");
  if (copy.Kinds.size() != 2 || copy.Instances.size() != 2) { return Report(); }
  for (size_t i = 0; i < source.Kinds.size(); ++i) {
    const auto &a = source.Kinds[i];
    const auto &b = copy.Kinds[i];
    CHECK(a.Name == b.Name && a.Inherits == b.Inherits && a.Asset == b.Asset &&
              a.Capabilities == b.Capabilities,
          "prefab identity inheritance asset and capabilities survive");
    CheckSettings(a.Attributes, b.Attributes);
    CHECK(a.Minds.size() == b.Minds.size(), "ordered mind count survives");
    if (a.Minds.size() != b.Minds.size()) { continue; }
    for (size_t j = 0; j < a.Minds.size(); ++j) {
      const auto &v = a.Minds[j];
      const auto &w = b.Minds[j];
      CHECK(v.Tier == w.Tier && v.Uses == w.Uses && v.Programme == w.Programme &&
                v.Prompt == w.Prompt && v.Model == w.Model && v.Meanwhile == w.Meanwhile &&
                v.Hz == w.Hz && v.EverySeconds == w.EverySeconds && v.StepBudget == w.StepBudget &&
                v.TokenBudget == w.TokenBudget && v.LatencyBudgetMs == w.LatencyBudgetMs &&
                v.Temperature == w.Temperature && v.Seed == w.Seed,
            "all mind metadata retains exact values");
    }
  }
  for (size_t i = 0; i < source.Instances.size(); ++i) {
    const auto &a = source.Instances[i];
    const auto &b = copy.Instances[i];
    CHECK(a.Of == b.Of && a.Id == b.Id && a.In == b.In && a.Holds == b.Holds,
          "instance identity type containment and ordered held references survive");
    CheckSettings(a.Attributes, b.Attributes);
    const auto &v = a.Stands;
    const auto &w = b.Stands;
    for (int axis = 0; axis < 3; ++axis) {
      CHECK(v.AtM[axis] == w.AtM[axis] && v.ScaleXyz[axis] == w.ScaleXyz[axis],
            "exact pose and scale survive");
    }
    CHECK(v.Facing.X == w.Facing.X && v.Facing.Y == w.Facing.Y && v.Facing.Z == w.Facing.Z &&
              v.Facing.W == w.Facing.W,
          "orientation remains unmodified");
    CHECK(v.GlobeAnchor == w.GlobeAnchor && v.SamplesHeight == w.SamplesHeight &&
              v.Geodetic.LongitudeDeg == w.Geodetic.LongitudeDeg &&
              v.Geodetic.LatitudeDeg == w.Geodetic.LatitudeDeg &&
              v.Geodetic.HeightM == w.Geodetic.HeightM && v.BearingDeg == w.BearingDeg &&
              v.PitchDeg == w.PitchDeg,
          "geographic placement retains its independent values");
  }
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<kinds>") == std::string::npos &&
            empty->find("<instances>") == std::string::npos,
        "empty catalogs remain absent");
  return Report();
}
