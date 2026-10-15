#ifndef OUTSHINE_TEST_EXPERIMENTS_WEATHER_CLOUDVIEWPROBE_H
#define OUTSHINE_TEST_EXPERIMENTS_WEATHER_CLOUDVIEWPROBE_H

#include "MediumViewProbe.h"
#include "LowCloud.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace outshine::Test::CloudViewProbe {
using namespace Render;
using namespace Render::Native;
using MediumViewProbe::Pixel;

struct alignas(16) WeatherFrame {
  Vec4f Shell;
  Vec4f Field;
  Vec4f Drift;
  Vec4f DensityCameraOriginM;
  Vec4f SunIrradiance;
  Vec4f SkyRadiance;
};

static_assert(sizeof(WeatherFrame) == 6 * sizeof(Vec4f) && alignof(WeatherFrame) == 16);
static_assert(offsetof(WeatherFrame, Shell) == 0 &&
              offsetof(WeatherFrame, Field) == sizeof(Vec4f) &&
              offsetof(WeatherFrame, Drift) == 2 * sizeof(Vec4f) &&
              offsetof(WeatherFrame, DensityCameraOriginM) == 3 * sizeof(Vec4f) &&
              offsetof(WeatherFrame, SunIrradiance) == 4 * sizeof(Vec4f) &&
              offsetof(WeatherFrame, SkyRadiance) == 5 * sizeof(Vec4f));

inline constexpr double kCloudViewPhaseS = 37.0;

struct Case {
  std::string Name;
  MediumViewProbe::Scenario View;
  WeatherSnapshot Snapshot;
  Vec3 DensityCameraOriginM;
  double PhaseS = kCloudViewPhaseS;
};

struct Capture {
  std::array<std::vector<Pixel>, 5> Images;
  WeatherFrame Weather;
};

[[nodiscard]] Medium ControlledAir();
[[nodiscard]] std::vector<Case> Cases(MediumViewProbe::ImageShape shape);
[[nodiscard]] WeatherFrame PrepareWeather(const Case &scenario);

struct Pipelines {
  const GraphicsPipeline &Scene;
  const GraphicsPipeline &Front;
};

class Targets {
public:
  explicit Targets(Device &device, MediumViewProbe::ImageShape shape);
  [[nodiscard]] Capture
  Draw(Device &device, const Pipelines &pipelines, const Case &scenario) const;

private:
  void Scene(Commands &commands, const GraphicsPipeline &pipeline, const Case &scenario) const;
  std::array<std::unique_ptr<Texture>, 4> Inputs_;
  std::array<std::unique_ptr<Texture>, 4> Outputs_;
  std::array<std::unique_ptr<Buffer>, 5> Readbacks_;
  std::unique_ptr<Sampler> Exact_, Linear_;
  MediumViewProbe::ImageShape Shape_;
};

void WriteCapture(const Case &scenario,
                  const Capture &capture,
                  MediumViewProbe::ImageShape shape,
                  const std::string &prefix);
}
#endif
