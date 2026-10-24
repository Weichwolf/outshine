#ifndef OUTSHINE_TEST_CLIENTSHOT_H
#define OUTSHINE_TEST_CLIENTSHOT_H

#include "Check.h"
#include "ClientCommand.h"
#include "format/Json.h"
#include "io/PngImage.h"
#include "io/ReadTextFile.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test {

inline constexpr uint32_t kPlaceWidthPx = 1280;
inline constexpr uint32_t kPlaceHeightPx = 720;
inline constexpr unsigned kPlaceRateHz = 60;

struct PlaceCapture {
  Json Summary;
  Json Report;
};

inline double PlaceMetric(Json::Ref report, const char *name) {
  const auto metrics = report["metrics"];
  for (size_t at = 0; at < metrics.Size(); ++at) {
    if (metrics[at]["name"].StrEquals(name)) {
      return metrics[at]["value"].Num(std::numeric_limits<double>::quiet_NaN());
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

inline double RowVariation(const Io::Png &image) {
  if (!image.Read || image.Wide < 2 || image.High == 0 || image.Channels < 3) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double sum = 0;
  size_t steps = 0;
  for (size_t y = 0; y < image.High; ++y) {
    for (size_t x = 1; x < image.Wide; ++x) {
      const size_t at = (y * image.Wide + x) * image.Channels;
      for (size_t channel = 0; channel < 3; ++channel) {
        const int here = image.Bytes[at + channel];
        const int left = image.Bytes[at - image.Channels + channel];
        sum += std::abs(here - left);
        ++steps;
      }
    }
  }
  return sum / static_cast<double>(steps);
}

inline bool CheckPlacePicture(const PlaceCapture &capture, const char *place) {
  const auto summary = capture.Summary.Root();
  const auto report = capture.Report.Root();
  constexpr double kMinimumVisibleVariation = 0.1;
  constexpr size_t kMostImageBytes = size_t{16} * 1024 * 1024;
  const auto bytes = ReadTextFile(summary["image"].Str(), kMostImageBytes);
  if (!bytes) {
    Unprepared(bytes.error().c_str());
    return false;
  }
  const auto image = Io::ReadPng(reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size());
  CHECK(image.Read && image.Wide == kPlaceWidthPx && image.High == kPlaceHeightPx,
        "the saved image has the requested dimensions and valid pixels");
  Io::Png gradient{.Read = true,
                   .Wide = kPlaceWidthPx,
                   .High = 2,
                   .Channels = 4,
                   .Bytes = std::vector<uint8_t>(size_t{kPlaceWidthPx} * 2 * 4,
                                                 std::numeric_limits<uint8_t>::max()),
                   .Error = {}};
  for (size_t at = 0; at < size_t{kPlaceWidthPx} * 4; ++at) { gradient.Bytes[at] = 0; }
  const double control = RowVariation(gradient);
  const double variation = RowVariation(image);
  CHECK(std::isfinite(control) && control >= 0 && control < 0.5,
        "a vertical gradient is rejected by the actual image variation measure");
  const double bare = PlaceMetric(report, "tiles the last rebuild laid bare");
  CHECK(std::isfinite(bare) && bare == 0, "no terrain tiles remain without elevation");
  const auto digest = summary["sha256"].Str().substr(0, 8);
  const auto reference =
      std::filesystem::path("build/shots/reference") / (std::string(place) + "-" + digest + ".png");
  const double triangles = PlaceMetric(report, "building triangles the world meshed");
  CHECK(std::isfinite(triangles) && triangles >= 0, "the building geometry measure is present");
  if (!std::filesystem::exists(reference) && triangles > 0) {
    CHECK(std::isfinite(variation) && variation > control + kMinimumVisibleVariation,
          "meshed buildings produce a picture distinct from the blank-frame control");
  }
  return image.Read;
}

inline void CheckPlaceTiming(const PlaceCapture &capture, unsigned rateHz) {
  const auto summary = capture.Summary.Root();
  const auto report = capture.Report.Root();
  constexpr double kBudgetRoundingMs = 1e-6;
  constexpr double kClockRoundingS = 1e-12;
  constexpr double kMinimumPacedWindowMs = 999.0;
  constexpr double kCurrentCpuBudgetMs = 10.0;
  const auto profile = report["profile"];
  const auto motion = report["motion"];
  const auto gpu = report["gpu_times"];
  const auto cpu = report["cpu_times"]["frame"];
  const auto phases = report["phases_ms"];
  const double missing = std::numeric_limits<double>::quiet_NaN();
  const double budgetMs = 1000.0 / rateHz;
  CHECK(std::cmp_equal(profile["width_px"].Int(), kPlaceWidthPx) &&
            std::cmp_equal(profile["height_px"].Int(), kPlaceHeightPx) &&
            profile["fps"].Int() == static_cast<int>(rateHz) &&
            std::abs(profile["budget_ms"].Num(missing) - budgetMs) < kBudgetRoundingMs,
        "the requested 1280x720 profile and frame budget are measured");
  CHECK(summary["frames"].Int() == static_cast<int>(rateHz) &&
            summary["turn_degrees"].Int() == 360 &&
            motion["frames"].Int() == static_cast<int>(rateHz) &&
            std::abs(motion["time_step_s"].Num(missing) * rateHz - 1.0) < kClockRoundingS,
        "exactly one second of target-rate simulation frames completes the camera turn");
  CHECK(phases["paced_window"].Num(missing) >= kMinimumPacedWindowMs,
        "the measured turn is paced over a complete second of actual wall time");
  CHECK(std::isfinite(phases["preload"].Num(missing)) && phases["preload"].Num(missing) >= 0,
        "complete cached world preload reports its actual duration");
  CHECK(report["measurement_complete"].Bool() && report["world_complete"].Bool() &&
            report["unrefined_frames"].Int(-1) == 0,
        "the complete world is present before and throughout measurement");
  CHECK(std::isfinite(cpu["p99_ms"].Num(missing)) && cpu["p99_ms"].Num(missing) > 0 &&
            cpu["p99_ms"].Num(missing) < kCurrentCpuBudgetMs,
        "current place CPU p99 stays below ten milliseconds, including initial frames");
  CHECK(gpu["complete"].Bool() && gpu["rendered_frames"].Int() == static_cast<int>(rateHz) &&
            gpu["timed_frames"].Int() == static_cast<int>(rateHz) &&
            gpu["unavailable_frames"].Int(-1) == 0,
        "every measured frame has a native GPU timestamp");
  CHECK(std::isfinite(gpu["frame"]["max_ms"].Num(missing)) &&
            gpu["frame"]["max_ms"].Num(missing) > 0 &&
            gpu["frame"]["max_ms"].Num(missing) <= budgetMs,
        "GPU maximum and p99 fit the requested frame budget");
}

inline int ScorePlace(const char *place,
                      const char *catalog = "src/assets/places",
                      unsigned rateHz = kPlaceRateHz) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const auto command = "python3 -B test/scripts/places.py --catalog " + ShellArgument(catalog) +
                       " --client " + ClientCommand() + " --fps " + std::to_string(rateHz) +
                       " --preload-seconds 600 " + ShellArgument(place) + " 2>&1";
  std::string output;
  const int status = Run(command, output);
  std::print("{}", output);
  PlaceCapture capture;
  if (status != 0 || !capture.Summary.Parse(output.data(), output.size())) {
    Unprepared(
        (std::string(place) + " capture failed with status " + std::to_string(status)).c_str());
    return Report();
  }
  constexpr size_t kMostReportBytes = size_t{4} * 1024 * 1024;
  const auto bytes = ReadTextFile(capture.Summary.Root()["report"].Str(), kMostReportBytes);
  if (!bytes || !capture.Report.Parse(bytes->data(), bytes->size())) {
    Unprepared("the native benchmark left no valid report");
    return Report();
  }
  CHECK(capture.Summary.Root()["place"].StrEquals(place), "the requested place was captured");
  CHECK(capture.Report.Root()["benchmark_version"].Int() == 1 &&
            capture.Report.Root()["mode"].StrEquals("scenario"),
        "the native scenario benchmark provides the measurement");
  CheckPlaceTiming(capture, rateHz);
  if (!CheckPlacePicture(capture, place)) { Unprepared("the driver left no valid final picture"); }
  return Report();
}

}
#endif
