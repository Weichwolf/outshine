#ifndef OUTSHINE_TEST_PREPAREDROADPAGEFIXTURE_H
#define OUTSHINE_TEST_PREPAREDROADPAGEFIXTURE_H
#include "PreparedRoadPages.h"
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Test {
inline std::expected<Generators::RoadPageLease, std::string>
PrepareAndLoadRoadPage(Generators::PreparedRoadPages &pages,
                       const Generators::RoadPageRequest &request,
                       const Generators::RoadPageProducer *producer = nullptr,
                       const std::stop_token &stop = {}) {
  const auto prepared = pages.Prepare(request, producer, stop);
  if (!prepared) { return std::unexpected(prepared.error()); }
  auto loaded = request.RequestKey.empty()
                    ? pages.Load(request.Address, request.NativeBytesMost)
                    : pages.LoadRequest(request.RequestKey, request.NativeBytesMost);
  if (!loaded) {
    return std::unexpected(Generators::PreparedRoadPages::DescribeFailure(loaded.error()));
  }
  if (!*loaded) { return std::unexpected("prepared native road page is unavailable"); }
  return std::move(**loaded);
}
}
#endif
