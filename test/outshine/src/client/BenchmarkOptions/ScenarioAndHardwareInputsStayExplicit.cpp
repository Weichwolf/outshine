#include "BenchmarkCommand.h"
#include "BenchmarkOptions.h"
#include "Check.h"

#include <array>
#include <span>
#include <string_view>

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  const auto hardware =
      ReadBenchmarkOptions(std::array{"--output", "result.json", "--gpu-validation"});
  CHECK(hardware && !hardware->Scenario && hardware->GpuValidation &&
            hardware->Output == "result.json",
        "an output path and validation alone preserve synthetic calibration");
  const auto parsed = ReadBenchmarkOptions(std::array{"--scenario",
                                                      "scene.json",
                                                      "--frames",
                                                      "60",
                                                      "--linear-velocity",
                                                      "1,-2,3",
                                                      "--angular-velocity",
                                                      "0,-360,0",
                                                      "--view",
                                                      "start",
                                                      "--offline",
                                                      "--output",
                                                      "report.json",
                                                      "--image",
                                                      "last.png"});
  CHECK(parsed && parsed->Scenario,
        "a complete scenario measurement is parsed without device work");
  if (parsed && parsed->Scenario) {
    const auto &scenario = *parsed->Scenario;
    CHECK(scenario.Source == "scene.json" && scenario.Motion.Frames == 60 &&
              scenario.View == "start" && scenario.Offline && scenario.Image == "last.png" &&
              parsed->Output == "report.json",
          "scenario, time window, camera and outputs retain their identities");
    CHECK(scenario.Motion.LinearVelocityMps[0] == 1 && scenario.Motion.LinearVelocityMps[1] == -2 &&
              scenario.Motion.LinearVelocityMps[2] == 3 &&
              scenario.Motion.AngularVelocityDegS[1] == -360,
          "signed world-axis velocities are not converted to per-frame increments");
  }
  const auto reject = [](std::span<const char *const> arguments) {
    CHECK(!ReadBenchmarkOptions(arguments),
          "invalid or ambiguous inputs fail before runtime startup");
  };
  reject(std::array<const char *, 1>{nullptr});
  reject(std::array<const char *, 2>{"--scenario", nullptr});
  reject(std::array{"--frames", "60"});
  reject(std::array{"--scenario", "scene.json"});
  reject(std::array{"--scenario", "scene.json", "--frames", "1", "--gpu-validation"});
  reject(std::array{
      "--scenario", "scene.json", "--frames", "1", "--output", "same", "--image", "same"});
  reject(std::array{"--gpu-validation", "--gpu-validation"});
  reject(std::array{"--offline", "--offline"});
  reject(std::array{"--output", "a", "--output", "b"});
  reject(std::array{"--unknown"});
  for (const char *number : {"0", "-1", "1.0", "1000001", "18446744073709551616", "1extra", ""}) {
    reject(std::array{"--scenario", "scene.json", "--frames", number});
  }
  for (const char *vector :
       {"1,2", "1,2,3,4", "nan,0,0", "0,inf,0", "0,,1", "0,0,1junk", "", "1e999,0,0"}) {
    reject(std::array{"--scenario", "scene.json", "--frames", "1", "--linear-velocity", vector});
    reject(std::array{"--scenario", "scene.json", "--frames", "1", "--angular-velocity", vector});
  }
  for (const char *seconds : {"0", "-1", "nan", "inf", "1extra"}) {
    reject(std::array{"--scenario", "scene.json", "--frames", "1", "--preload-seconds", seconds});
  }
  reject(std::array{"--scenario", "scene.json", "--frames", "1", "--scenario-overrides", "[]"});
  const auto maximum =
      ReadBenchmarkOptions(std::array{"--scenario", "scene.json", "--frames", "1000000"});
  CHECK(maximum && maximum->Scenario->Motion.Frames == 1000000,
        "the declared bounded frame limit is inclusive");
  return Report();
}
