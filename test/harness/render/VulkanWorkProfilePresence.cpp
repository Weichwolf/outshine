#include "VulkanWorkProfile.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <print>

namespace {
namespace Probe = outshine::Render::Native::Vulkan::WorkProbe;

[[nodiscard]] bool FragmentCounter(std::array<bool, 2> stages) {
  Probe::Sample sample;
  for (const bool fragment : stages) {
    Probe::Pipelines[VK_NULL_HANDLE] = {
        .Device = VK_NULL_HANDLE, .Label = "metadata-only", .Fragment = fragment};
    Probe::Identify(sample, VK_NULL_HANDLE);
  }
  return sample.Fragment;
}
}

int main() {
  constexpr std::array cases{std::array{false, false},
                             std::array{false, true},
                             std::array{true, false},
                             std::array{true, true}};
  constexpr std::array expected{false, false, false, true};
  size_t failed = 0;
  for (size_t i = 0; i < cases.size(); ++i) {
    if (FragmentCounter(cases[i]) != expected[i]) {
      std::println(stderr, "fragment availability case {} failed", i);
      ++failed;
    }
  }
  Probe::Pipelines.clear();
  std::println(
      "{} of {} metadata controls passed; no GPU execution", cases.size() - failed, cases.size());
  return failed == 0 ? 0 : 1;
}
