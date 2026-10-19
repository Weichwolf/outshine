#ifndef OUTSHINE_TEST_PREPARATIONCOMMON_H
#define OUTSHINE_TEST_PREPARATIONCOMMON_H

#include "Check.h"
#include "ImpostorPreparation.h"
#include "ImpostorSurface.h"
#include <Outshine.h>
#include <scenario/Scenario.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

namespace outshine::Test::CrownFixture {
using namespace Generators;
constexpr int kPixels = 128;
constexpr size_t kViews = 4;
constexpr Extent kCardExtent{.WidthPx = kPixels, .HeightPx = kPixels};
constexpr Extent kForegroundExtent{.WidthPx = 1280, .HeightPx = 720};
constexpr Extent kInstanceExtent{.WidthPx = 3 * kPixels, .HeightPx = kPixels};
constexpr size_t kTexels = size_t{kPixels} * kPixels;
constexpr size_t kChannels = 4;
constexpr uint8_t kOpaque = 255;
constexpr float kByteSteps = 255;
constexpr float kNormalBound = .003f;
constexpr double kLightLux = 20000;
constexpr double kElevationDeg = 40;
constexpr std::array<double, 2> kBearings{135, 315};
constexpr size_t kForegroundFrames = 120;
constexpr auto kCaptureTimeout = std::chrono::seconds(30);
constexpr auto kLoadTimeout = std::chrono::seconds(5);
constexpr auto kPollInterval = std::chrono::milliseconds(1);
constexpr double kFrameBudgetMs = 1000.0 / 60;
constexpr std::array<double, 3> kQuantiles{.5, .95, .99};
constexpr size_t kChecksumBytes = 8;
constexpr size_t kCorruptByte = 100;
constexpr size_t kShortHeaderBytes = 71;
constexpr std::array<size_t, 4> kCorruptFields{8, 20, 24, 28};
constexpr std::array<uint8_t, 4> kNanBytes{0, 0, 192, 127};
constexpr size_t kNanOffset = 64;
constexpr double kBoundsM = 100;
constexpr size_t kMissingPolls = 20;
constexpr float kHalfStep = 0x1p-10f;
constexpr float kHalfSmallest = 0x1p-24f;

inline std::string OutputDirectory() {
  return (std::filesystem::path(OUTSHINE_TEST_NEST) / "crown-atlas").string();
}

inline void WriteBytes(std::span<const uint8_t> bytes, const std::string &path) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(output.good(), "the verified crown diagnostic artifact is written");
}
}
#endif
