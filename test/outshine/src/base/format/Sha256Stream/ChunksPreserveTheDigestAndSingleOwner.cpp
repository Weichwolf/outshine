#include "Sha256.h"
#include "Sha256Stream.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

static_assert(!std::is_copy_constructible_v<Sha256Stream>);
static_assert(!std::is_copy_assignable_v<Sha256Stream>);
static_assert(std::is_nothrow_destructible_v<Sha256Stream>);

void CheckBoundaries() {
  constexpr std::array<size_t, 17> lengths{
      0, 1, 55, 56, 63, 64, 65, 1023, 1024, 1025, 2047, 2048, 2049, 4095, 4096, 4097, 65537};
  constexpr std::array<size_t, 10> chunks{1, 3, 55, 64, 65, 511, 1023, 1024, 1025, 8192};
  for (const size_t length : lengths) {
    std::vector<uint8_t> bytes(length);
    for (size_t at = 0; at < bytes.size(); ++at) { bytes[at] = static_cast<uint8_t>(at % 251); }
    const auto expected = Sha256Prefix(nullptr, 0).Digest(bytes.data(), bytes.size());
    for (const size_t chunk : chunks) {
      Sha256Stream hash;
      hash.Append(nullptr, 0);
      for (size_t at = 0; at < bytes.size();) {
        const size_t count = std::min(chunk, bytes.size() - at);
        hash.Append(bytes.data() + at, count);
        at += count;
      }
      const auto digest = hash.Finish();
      CHECK(digest && *digest == expected,
            "streamed chunks preserve independently validated SHA and staging-block boundaries");
      const auto repeated = hash.Finish();
      CHECK(!repeated && repeated.error() == Sha256StreamError::Finished,
            "a finalized stream explicitly rejects a second digest");
      hash.Append(bytes.data(), bytes.size());
      CHECK(!hash.Finish(), "append cannot revive a finalized stream");
    }
  }
}

void CheckParallel() {
  constexpr std::array<uint8_t, 3> bytes{'a', 'b', 'c'};
  const auto expected = Sha256Prefix(nullptr, 0).Digest(bytes.data(), bytes.size());
  std::atomic<size_t> failures{0};
  std::vector<std::jthread> threads;
  threads.reserve(8);
  for (size_t thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&] {
      for (size_t round = 0; round < 32; ++round) {
        Sha256Stream hash;
        for (const auto &byte : bytes) { hash.Append(&byte, sizeof(byte)); }
        const auto digest = hash.Finish();
        if (!digest || *digest != expected) { ++failures; }
      }
    });
  }
  threads.clear();
  CHECK(failures == 0, "parallel streams own independent provider states and bounded scratch");
}
}

int main() {
  CheckParallel();
  CheckBoundaries();
  return Report();
}
