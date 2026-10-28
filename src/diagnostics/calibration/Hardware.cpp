#include "Calibration.h"
#include "CalibrationBuild.h"
#include "SpirvFile.h"
#include "ComputeShaders.h"
#include "Parameters.inc"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Diagnostics::Calibration {
static_assert(Render::FindComputeShader(Render::ComputeShaderId::CalibrationFma)->Shape.GroupX ==
              CALIBRATION_LOCAL_SIZE);
static_assert(Render::FindComputeShader(Render::ComputeShaderId::CalibrationTriad)->Shape.GroupX ==
              CALIBRATION_LOCAL_SIZE);

namespace {

std::string HostLoad() {
  std::array<double, 3> values{};
  if (getloadavg(values.data(), static_cast<int>(values.size())) !=
      static_cast<int>(values.size())) {
    return "unavailable";
  }
  return std::format("load averages: 1m={}, 5m={}, 15m={}", values[0], values[1], values[2]);
}

}

Native::Result<Report> Run(bool gpuValidation) {
  constexpr std::array files{"calibrationFma.comp.spv",
                             "calibrationTriad.comp.spv",
                             "calibrationRaster.vert.spv",
                             "calibrationRaster.frag.spv"};
  std::array<std::vector<uint32_t>, files.size()> words;
  for (size_t at = 0; at < files.size(); ++at) {
    auto loaded = Render::ReadShaderSpirv(files[at]);
    if (!loaded) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = loaded.error()});
    }
    words[at] = std::move(*loaded);
  }
  auto device =
      Native::CreateDevice({.Validation = gpuValidation,
                            .GpuTiming = outshine::Render::Native::GpuTimingMode::Submission});
  if (!device) { return std::unexpected(device.error()); }
  auto report = Run(**device,
                    {.Fma = words[0], .Triad = words[1], .Vertex = words[2], .Fragment = words[3]},
                    {.Commit = std::string(kBuildCommit),
                     .Flags = std::string(kBuildFlags),
                     .HostLoad = HostLoad()});
  if (report) { report->GpuValidation = gpuValidation; }
  return report;
}

}
