#ifndef OUTSHINE_APP_BENCHMARKOPTIONS_H
#define OUTSHINE_APP_BENCHMARKOPTIONS_H

#include "BenchmarkMotion.h"
#include "ScenarioArguments.h"
#include <optional>
#include <string_view>

namespace outshine::App {

struct ScenarioBenchmarkOptions {
  std::string_view Source;
  std::string_view Overrides;
  std::string_view View;
  std::string_view Image;
  std::string_view GeometryDirectory;
  std::string_view PanoramaDirectory;
  std::string_view TileProduct;
  std::string_view PanoramaProduct;
  std::string_view NextPanorama;
  std::string_view CacheDirectory;
  std::string_view AssetCacheDirectory;
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
