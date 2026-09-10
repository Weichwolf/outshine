#include "ScenarioLayer.h"
#include "Check.h"
#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Layer layer{.Id = "label", .Path = "unused", .Set = "night"};
  CHECK(LayerActive(layer, "  day night  "), "whole token selected across spaces");
  CHECK(!LayerActive(layer, "midnight Night day\tnight"),
        "no substring, case folding or tab splitting");
  layer.Set.clear();
  CHECK(LayerActive(layer, ""), "unconditional layer selected");
  Scenario::Document into;
  into.Named.Name = "base";
  into.Motion.StepS = 0.02;
  into.Motion.MostStepsInArrears = 8;
  into.Events.push_back({.Name = "old", .Carries = {"before"}});
  std::vector<std::string> trace{"previous"};
  std::string error;
  for (const std::string_view text : {"<scenario><events><event name=\"new\"/></events><physics "
                                      "mostStepsInArrears=\"bad\"/></scenario>",
                                      "<scenario><events><event name=\"new\"/></events><layer "
                                      "path=\"nested.scn\"/></scenario>"}) {
    CHECK(!ApplyLayer(into, text.data(), text.size(), "bad", trace, error),
          "invalid layer rejected");
    CHECK(!error.empty(), "failure diagnosed");
    CHECK(into.Named.Name == "base" && into.Events.size() == 1 &&
              into.Events.front().Name == "old" &&
              into.Events.front().Carries == std::vector<std::string>{"before"} &&
              into.Motion.StepS == 0.02 && into.Motion.MostStepsInArrears == 8,
          "rejected layer preserves document");
    CHECK(trace == std::vector<std::string>{"previous"}, "rejected layer preserves trace");
  }
  constexpr std::string_view valid =
      "<scenario><events><event name=\"old\"><carries what=\"after\"/></event>"
      "<event name=\"new\"/></events><physics stepS=\"0.01\"/></scenario>";
  CHECK(ApplyLayer(into, valid.data(), valid.size(), "good", trace, error), "valid retry succeeds");
  CHECK(error.empty() && into.Events.size() == 2 &&
            into.Events.front().Carries == std::vector<std::string>{"after"} &&
            into.Events.back().Name == "new",
        "matching event replaced and new event appended");
  CHECK(into.Motion.StepS == 0.01 && into.Motion.MostStepsInArrears == 8,
        "omitted scalar preserves base value");
  CHECK(trace.size() > 1 && trace.front() == "previous", "successful merge appends trace");
  return Report();
}
