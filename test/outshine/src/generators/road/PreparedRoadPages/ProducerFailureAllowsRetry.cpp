#include "PreparedRoadPageFixture.h"
#include "Earth.h"
#include "ControlledRoadPageProducer.h"
#include <stop_token>
#include "PreparedRoadPages.h"
#include "Check.h"
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

RoadPageAddress Address(uint32_t part) {
  return {.PlanKey = std::string(64, 'a'),
          .MaterialKey = std::string(64, 'b'),
          .MaterialIndex = 0,
          .Anchor = kAnchor,
          .Level = 0,
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}};
}

RoadPageGeometry Triangle(uint32_t part) {
  return {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                   .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                   .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                   .Index = {0, 1, 2}},
          .Swept = {},
          .Diagnostics = {},
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}};
}

void CheckRetry(PreparedRoadPages &pages) {
  using namespace outshine::Test;
  size_t attempts = 0;
  const auto failed = outshine::Test::PrepareAndLoadRoadPage(
      pages,
      {.Address = Address(9), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &)
                                       -> std::expected<RoadPageGeometry, std::string> {
        ++attempts;
        return std::unexpected("controlled producer failure");
      }).get());
  CHECK(!failed, "a failed build does not publish a complete page");
  const auto retry = outshine::Test::PrepareAndLoadRoadPage(
      pages,
      {.Address = Address(9), .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
      outshine::Test::RoadProducer([&](const RoadPageRequest &, const std::stop_token &) {
        ++attempts;
        return Triangle(9);
      }).get());
  CHECK(retry && attempts == 2, "a later demand can retry after the failed producer is released");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-road-page-retry-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  auto opened = PreparedRoadPages::Open(root.string());
  CHECK(opened.has_value(), "the existing native cache opens");
  if (!opened) { return Report(); }
  auto pages = std::shared_ptr<PreparedRoadPages>(std::move(*opened));
  CheckRetry(*pages);
  pages.reset();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the retry cache fixture is cleaned after closing");
  return Report();
}
