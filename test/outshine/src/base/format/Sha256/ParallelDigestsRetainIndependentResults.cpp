#include "Sha256.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  // Independent hashlib.sha256 vectors for the ASCII messages parallel/0 .. parallel/7.
  constexpr std::array<std::string_view, 8> expected{
      "68f4e0f84cb502bc069b0c288a66a1127210b8a0de8e5450d42a3ce3d982e02b",
      "b5b1eb816272fe9e5a8141f81b0e5a056fc5bd5921e8ce63afda73f9a29e580a",
      "af6979167e8edf7e0e51aa7b40c12db3e4578929e46eed0f96c83da7fee3b1ab",
      "3dfc488dce3d71517818ca1398b1e8ca54bcf67b61669b77286b1104ff18f964",
      "18bcd82450a61851a3509fa77c515916211ed551b13267c4ddcadc41d85485df",
      "7df2a1c540adf546693150c12f3067edd43e89879e8ab3dc65eb74d9b528ceb5",
      "5f25142fe5c2f000b681959dc79fe5049999d19c35a88d653e644e69d926dc08",
      "31022f75d35b00d749dc38fa59bd7e740addbd2bb04b741fa4d306dd43ade784",
  };
  constexpr size_t kRepetitions = 200;
  const Sha256Prefix prefix("parallel/", 9);
  std::array<bool, expected.size()> matched{};
  {
    std::array<std::jthread, expected.size()> workers;
    for (size_t at = 0; at < workers.size(); ++at) {
      workers[at] = std::jthread([&, at] {
        const auto suffix = std::to_string(at);
        const auto message = std::string("parallel/") + suffix;
        bool correct = true;
        for (size_t repeat = 0; repeat < kRepetitions; ++repeat) {
          correct &= Sha256Hex(message) == expected[at];
          correct &= prefix.Hex(suffix.data(), suffix.size()) == expected[at];
        }
        matched[at] = correct;
      });
    }
  }
  for (const auto correct : matched) {
    CHECK(correct,
          "concurrent whole-message and shared const prefix digests retain independent results");
  }
  return Report();
}
