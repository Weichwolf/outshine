#include "PreparedRoadPages.h"
#include "StoredRoadPageMeshProducer.h"
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kPageBytesMost = size_t{64} * 1024 * 1024;
constexpr RoadDisplayLimits kProbeLimits{.StationsMost = 65536, .PoseQueriesMost = 131072};

std::shared_ptr<PreparedRoadPages> Open(const std::string &path) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return {}; }
  auto pages = PreparedRoadPages::Open(std::move(*cache));
  if (!pages) { return {}; }
  return std::move(*pages);
}

std::expected<RoadPageLease, std::string> Generate(const std::shared_ptr<PreparedRoadPages> &source,
                                                   PreparedRoadPages &output,
                                                   RoadPageAddress address,
                                                   double errorM) {
  address.Level = 1;
  address.PositionErrorM = errorM;
  address.DisplayPolicy = RoadDisplayPolicy::StoredSurfaceEnvelope;
  const auto read = source->PlanRecipeRead(address, kPageBytesMost);
  if (!read) { return std::unexpected(PreparedRoadPages::DescribeFailure(read.error())); }
  if (!*read) { return std::unexpected("missing recipe"); }
  auto producer = StoredRoadPageMeshProducer::Bind(source, address, **read);
  if (!producer) { return std::unexpected(producer.error()); }
  const RoadPageRequest request{.Address = std::move(address),
                                .Parent = {},
                                .RequestKey = {},
                                .NativeBytesMost = kPageBytesMost,
                                .SweepLimits = kProbeLimits};
  const auto record = output.Prepare(request, &*producer);
  if (!record) { return std::unexpected(record.error()); }
  auto page = output.LoadKey(record->Key, kPageBytesMost);
  if (!page) { return std::unexpected(PreparedRoadPages::DescribeFailure(page.error())); }
  if (!*page) { return std::unexpected("missing generated page"); }
  return std::move(**page);
}
}

int main(int argc, const char *const *argv) {
  std::error_code error;
  if (argc != 4 || !std::filesystem::is_regular_file(argv[1], error) || error ||
      std::filesystem::exists(argv[3], error) || error) {
    std::cerr << "usage: road_display source.sqlite manifest-key new-output.sqlite < flight-cut\n";
    return 2;
  }
  const auto source = Open(argv[1]);
  const auto output = Open(argv[3]);
  if (!source || !output) { return 3; }
  const auto manifest = source->LoadManifest(argv[2]);
  if (!manifest || !*manifest) { return 4; }
  const auto &original = ***manifest;
  std::unordered_map<std::string, const RoadPageReference *> pages;
  pages.reserve(original.Pages.size());
  for (const auto &page : original.Pages) { pages.emplace(page.Key, &page); }

  std::cout << "key\terror_m\toriginal_bytes\tdisplay_bytes\toriginal_triangles\tdisplay_"
               "triangles\tresult\n";
  std::string key;
  while (std::cin >> key) {
    double errorM = 0;
    const auto found = pages.find(key);
    if (!(std::cin >> errorM) || !std::isfinite(errorM) || errorM <= 0 || found == pages.end()) {
      return 5;
    }
    const auto &page = *found->second;
    const auto display = Generate(source, *output, StoredRoadPageAddress(original, page), errorM);
    const auto fine = source->LoadKey(key, kPageBytesMost);
    if (!fine || !*fine) {
      std::cerr << "missing comparison page " << key << '\n';
      return 6;
    }
    const auto fineTriangles = (**fine).Page->Geometry.Mesh.Index.size() / 3;
    const auto displayTriangles = display ? display->Page->Geometry.Mesh.Index.size() / 3 : 0;
    std::cout << key << '\t' << errorM << '\t' << page.ByteCount << '\t'
              << (display ? display->Record.ByteCount : 0) << '\t' << fineTriangles << '\t'
              << displayTriangles << '\t' << (display ? "ok" : display.error()) << '\n';
    pages.erase(found);
  }
  return std::cin.eof() ? 0 : 5;
}
