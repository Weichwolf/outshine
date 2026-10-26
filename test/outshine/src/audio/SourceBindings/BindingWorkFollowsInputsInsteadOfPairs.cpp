#include "SourceBindings.h"
#include "Mixer.h"
#include "BusGraph.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine::Audio;

void Scan(std::span<const SoundSource> declared,
          std::span<const Heard> heard,
          std::span<const Heard *> resolved) {
  std::ranges::fill(resolved, nullptr);
  for (size_t row = 0; row < declared.size(); ++row) {
    for (const Heard &source : heard) {
      if (source.Id == declared[row].Id && source.Standing) { resolved[row] = &source; }
    }
  }
}
}

int main() {
  using namespace outshine::Test;
  using Clock = std::chrono::steady_clock;
  for (const size_t count : {size_t{64}, size_t{256}, size_t{1024}}) {
    std::vector<SoundSource> declared(count);
    std::vector<Heard> heard(count);
    for (size_t row = 0; row < count; ++row) {
      declared[row].Id = "sound-" + std::to_string(row);
      heard[row].Id = declared[row].Id;
      heard[row].Standing = true;
    }
    std::ranges::reverse(heard);
    std::array<MixBus, 1> buses{};
    buses[0].Id = "master";
    BusGraph routing;
    CHECK(routing.Build(buses, declared), "maximum supported source count configures");
    SourceBindings bindings;
    bindings.Prepare(declared, routing);
    std::vector<const Heard *> reference(count);
    std::array<double, 64> scannedUs{};
    std::array<double, 64> indexedUs{};
    for (size_t attempt = 0; attempt < indexedUs.size(); ++attempt) {
      const auto beforeScan = Clock::now();
      Scan(declared, heard, reference);
      const auto beforeIndex = Clock::now();
      const auto indexed = bindings.Resolve(heard);
      const auto afterIndex = Clock::now();
      scannedUs[attempt] =
          std::chrono::duration<double, std::micro>(beforeIndex - beforeScan).count();
      indexedUs[attempt] =
          std::chrono::duration<double, std::micro>(afterIndex - beforeIndex).count();
      CHECK(std::ranges::equal(indexed, reference), "prepared lookup matches old scan identities");
      CHECK(bindings.Cost().IndexLookups == count && bindings.Cost().ClearedRows == count,
            "binding visits input rows rather than source pairs");
    }
    std::ranges::sort(scannedUs);
    std::ranges::sort(indexedUs);
    std::println("audio-bindings sources={} scan-pairs={} index-queries={} "
                 "scan-p50-us={:.3f} index-p50-us={:.3f} index-p99-us={:.3f}",
                 count,
                 count * count,
                 bindings.Cost().IndexLookups,
                 scannedUs[32],
                 indexedUs[32],
                 indexedUs.back());
  }
  return Report();
}
