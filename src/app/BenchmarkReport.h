#ifndef OUTSHINE_APP_BENCHMARKREPORT_H
#define OUTSHINE_APP_BENCHMARKREPORT_H

#include "BenchmarkMotion.h"
#include "GpuFrameSample.h"
#include <Engine.h>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace outshine::App {

struct BenchmarkFrameSample {
  double WallMs = 0;
  double AdvanceMs = 0;
  double RenderMs = 0;
  bool Refined = false;
};

struct ScenarioBenchmarkReport {
  std::string Scenario;
  std::string DeclarationSha256;
  std::string View;
  std::string Cpu;
  std::string Gpu;
  Extent Frame;
  BenchmarkMotionInput Motion;
  bool Unpaced = false;
  Camera Start;
  Camera Finish;
  Loading Prepared;
  Loading Finished;
  double SetupMs = 0;
  double PreloadMs = 0;
  double WindowMs = 0;
  double FlushMs = 0;
  double ImageMs = 0;
  double HardwareMs = 0;
  std::optional<std::array<double, 3>> LoadBefore;
  std::optional<std::array<double, 3>> LoadAfter;
  std::vector<BenchmarkFrameSample> Frames;
  std::vector<Render::GpuFrameSample> GpuSamples;
  std::vector<DiagnosticSample> Measures;
  std::string FinalWorldBlockers;
};

[[nodiscard]] std::string ScenarioBenchmarkJson(const ScenarioBenchmarkReport &report);

}

#endif
