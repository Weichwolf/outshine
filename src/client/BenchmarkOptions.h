#ifndef OUTSHINE_CLIENT_BENCHMARKOPTIONS_H
#define OUTSHINE_CLIENT_BENCHMARKOPTIONS_H

#include "BenchmarkMotion.h"
#include "ShotOptions.h"
#include <optional>
#include <string_view>

namespace outshine::Client {

struct ScenarioBenchmarkOptions {
  std::string_view Source;
  std::string_view Overrides;
  std::string_view View;
  std::string_view Image;
  std::string_view CacheDirectory;
  BenchmarkMotionInput Motion;
  double PreloadSeconds = kDefaultPreloadSeconds;
  bool Offline = false;
};

struct BenchmarkOptions {
  std::string_view Output;
  bool GpuValidation = false;
  std::optional<ScenarioBenchmarkOptions> Scenario;
};

}

#endif
