#ifndef OUTSHINE_TEST_URBANPRISMSLOT_FIXTURE_H
#define OUTSHINE_TEST_URBANPRISMSLOT_FIXTURE_H
#include "UrbanPrismSlot.h"
#include "test/outshine/src/generators/osm/UrbanPrismPreparation/PrismPreparationFixture.h"
#include <chrono>
#include <expected>
#include <string>
#include <thread>

namespace outshine::Test::PrismPreparationFixture {
inline std::expected<void, std::string>
Await(UrbanPrismSlot &slot,
      const std::string &request,
      const Generators::Osm::UrbanPrismPreparation::Capture &capture = {}) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    const auto result = slot.Advance(request, capture);
    if (!result) { return std::unexpected(result.error()); }
    if (slot.ReadyKey() == request) { return {}; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return std::unexpected("prism slot did not deliver the requested product");
}

}
#endif
