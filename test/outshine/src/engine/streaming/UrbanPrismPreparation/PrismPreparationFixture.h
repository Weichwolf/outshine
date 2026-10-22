#ifndef OUTSHINE_TEST_URBANPRISMPREPARATION_FIXTURE_H
#define OUTSHINE_TEST_URBANPRISMPREPARATION_FIXTURE_H

#include "UrbanPrismPreparation.h"
#include "UrbanPrismAsset.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace outshine::Test::PrismPreparationFixture {
struct Cache {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-prism-job-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  Cache() {
    std::error_code error;
    std::filesystem::create_directories(Directory, error);
  }

  ~Cache() {
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
  }

  [[nodiscard]] std::string Database() const { return (Directory / "assets.sqlite").string(); }
};

inline Generators::Osm::UrbanPrisms Product() {
  Generators::Osm::UrbanPrisms result;
  result.Geometry.Samples = std::make_shared<const std::vector<PrismSpan>>(
      std::vector<PrismSpan>{{.Lower = 17, .Upper = 29}});
  result.Geometry.Patches = {{.Columns = 1, .Rows = 1}};
  result.Placements.resize(1);
  result.Placements.front().EcefFromLocal.At(0, 3) = 6378137;
  return result;
}

inline UrbanPrismPreparation::Result Await(UrbanPrismPreparation &preparation,
                                           const UrbanPrismPreparation::Capture &capture = {}) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto result = preparation.Advance(capture);
    if (!result || *result) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return std::unexpected("prism preparation did not complete within five seconds");
}
}
#endif
