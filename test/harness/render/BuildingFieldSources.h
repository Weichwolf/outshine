#ifndef OUTSHINE_TEST_BUILDING_FIELD_SOURCES_H
#define OUTSHINE_TEST_BUILDING_FIELD_SOURCES_H
#include "test/harness/engine/NativeBuildingSources.h"
#include "BuildingSourceDemand.h"
#include <chrono>
#include <thread>

namespace outshine::Test::BuildingSources {
inline bool WaitFor(Experiments::BuildingSourceDemand &demand,
                    Fixture &fixture,
                    Experiments::BuildingSourceDemand::Phase phase) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    (void)demand.Advance(fixture.Queue, fixture.Stack, 1);
    if (demand.Entries().size() == 1 && demand.Entries()[0].Where == phase) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

}
#endif
