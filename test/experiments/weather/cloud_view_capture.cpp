#include "CloudViewProbe.h"
#include "MediumViewProbe.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace outshine::Test::CloudViewProbe {
namespace {
void Write(std::ofstream &file, std::span<const std::byte> bytes) {
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  CHECK(file.good(), "the native capture is saved completely");
}

struct ReceiverDepth {
  float WrittenDepth;
  double AlongViewM;
};

bool DepthMatches(const Case &scenario, ReceiverDepth input) {
  const auto written = input.WrittenDepth;
  const auto expectedM = input.AlongViewM;
  if (expectedM == 0.0) { return false; }
  const auto projection = scenario.View.Projection.Projection();
  const double clip =
      (projection[14] - projection[10] * expectedM) / (projection[15] - projection[11] * expectedM);
  const double range = scenario.View.Area.MaxDepth - scenario.View.Area.MinDepth;
  const double idealDepth = scenario.View.Area.MinDepth + range * clip;
  const auto clipFloat = static_cast<float>(clip);
  const double clipUlp =
      std::nextafter(clipFloat, std::numeric_limits<float>::infinity()) - clipFloat;
  const double writtenUlp =
      std::nextafter(written, std::numeric_limits<float>::infinity()) - written;
  return std::abs(idealDepth - written) <= 2.0 * (std::abs(range * clipUlp) + writtenUlp);
}

bool FiniteFields(const Capture &capture, size_t pixel) {
  for (const auto &image : capture.Images) {
    for (const float value : image[pixel]) {
      if (!std::isfinite(value)) { return false; }
    }
  }
  return true;
}

ReceiverDepth ReceiverFor(const Pixel &scene, float written) {
  constexpr std::array<double, 3> distancesM{500.0, 1600.0, 4500.0};
  constexpr std::array<float, 3> alphas{0.25f, 0.5f, 0.75f};
  for (size_t index = 0; index < alphas.size(); ++index) {
    if (scene[3] == alphas[index]) {
      return {.WrittenDepth = written, .AlongViewM = distancesM[index]};
    }
  }
  return {.WrittenDepth = written, .AlongViewM = 0.0};
}

bool Inside(const Case &scenario, size_t pixel, uint32_t width) {
  const auto &area = scenario.View.Area;
  const double x = static_cast<double>(pixel % width) + 0.5;
  const auto row = pixel / width;
  const double y = static_cast<double>(row) + 0.5;
  return x >= area.X && x < area.X + area.Width && y >= area.Y && y < area.Y + area.Height;
}
}

void WriteCapture(const Case &scenario,
                  const Capture &capture,
                  MediumViewProbe::ImageShape shape,
                  const std::string &prefix) {
  std::ofstream images(prefix + ".images.bin", std::ios::binary);
  for (const auto &image : capture.Images) { Write(images, std::as_bytes(std::span(image))); }
  const auto medium = ControlledAir();
  std::ofstream parameters(prefix + ".parameters.bin", std::ios::binary);
  Write(parameters, std::as_bytes(std::span(&medium, 1)));
  Write(parameters, std::as_bytes(std::span(&capture.Weather, 1)));
  std::ofstream rays(prefix + ".rays.bin", std::ios::binary);
  bool valid = true;
  size_t receivers = 0;
  size_t sky = 0;
  for (size_t pixel = 0; pixel < capture.Images[0].size(); ++pixel) {
    const auto &output = capture.Images[1][pixel];
    const auto &radiance = capture.Images[2][pixel];
    const auto &through = capture.Images[3][pixel];
    const auto &work = capture.Images[4][pixel];
    valid = valid && FiniteFields(capture, pixel);
    const auto row = pixel / shape.Width;
    std::array<double, 11> record{};
    if (Inside(scenario, pixel, shape.Width)) {
      valid = valid && output[3] == capture.Images[0][pixel][3];
      if (radiance[3] > 0.0f) {
        ++receivers;
        const auto ray =
            MediumViewProbe::ReferenceRay(scenario.View,
                                          {{static_cast<double>(pixel % shape.Width) + 0.5,
                                            static_cast<double>(row) + 0.5,
                                            radiance[3]}},
                                          medium);
        const auto origin = scenario.DensityCameraOriginM + ray.OriginM;
        record = {origin[0],
                  origin[1],
                  origin[2],
                  ray.Direction[0],
                  ray.Direction[1],
                  ray.Direction[2],
                  ray.RadiusKm,
                  ray.CosView,
                  ray.CosSun,
                  ray.ViewSun,
                  ray.DistanceKm};
        constexpr double absoluteToleranceKm = 2e-6;
        valid = valid && std::abs(through[3] - ray.DistanceKm) <= absoluteToleranceKm;
        valid = valid && DepthMatches(scenario, ReceiverFor(capture.Images[0][pixel], radiance[3]));
        valid = valid && work[0] >= 0.0f && work[1] >= 0.0f && work[2] >= 0.0f && work[3] >= -1.0f;
      } else {
        ++sky;
        valid = valid && output == capture.Images[0][pixel];
      }
    } else {
      valid = valid && output == Pixel{};
    }
    Write(rays, std::as_bytes(std::span(record)));
  }
  CHECK(valid && receivers > 0 && sky > 0,
        "real native depth, independent ray, alpha, sky passthrough and finite fields agree");
  std::ofstream metadata(prefix + ".json");
  const auto &view = scenario.View.Area;
  metadata << std::setprecision(std::numeric_limits<double>::max_digits10) << R"({"name":")"
           << scenario.Name << R"(","width":)" << shape.Width << R"(,"height":)" << shape.Height
           << R"(,"phase_s":)" << scenario.PhaseS << R"(,"viewport":[)" << view.X << ',' << view.Y
           << ',' << view.Width << ',' << view.Height << R"(],"snapshot_origin":)"
           << static_cast<int>(scenario.Snapshot.Source.Origin) << R"(,"snapshot_utc_s":)"
           << scenario.Snapshot.Values.ValidUtcS << R"(,"source_sha256":")"
           << scenario.Snapshot.Source.PayloadSha256 << "\"}\n";
  CHECK(metadata.good(), "source provenance and actual raster profile are saved");
}
}
