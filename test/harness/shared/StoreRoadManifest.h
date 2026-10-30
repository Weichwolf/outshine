#ifndef OUTSHINE_TEST_STOREROADMANIFEST_H
#define OUTSHINE_TEST_STOREROADMANIFEST_H
#include "PreparedRoadPages.h"
#include <expected>
#include <string>
#include <utility>

namespace outshine::Test {
inline std::expected<std::string, Generators::PreparedRoadPages::Failure>
StoreRoadManifest(Generators::PreparedRoadPages &pages,
                  const Generators::RoadPageManifest &manifest) {
  auto work = pages.PrepareManifest(manifest);
  if (!work) { return std::unexpected(std::move(work.error())); }
  return pages.StoreManifest(*work);
}
}
#endif
