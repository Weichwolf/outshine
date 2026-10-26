#include "Calibration.h"
#include "Parameters.inc"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <numeric>
#include <ratio>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace outshine::Diagnostics::Calibration {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kSmallTriadElements = 65536;
constexpr size_t kLargeTriadElements = 8388608;
constexpr uint32_t kSmallChainElements = 16384;
constexpr uint32_t kLargeChainElements = 16777216;
constexpr uint32_t kShuffleSeed = 0x6d2b79f5u;
constexpr uint32_t kShuffleRightShift = 17;
constexpr size_t kRounds = CALIBRATION_SAMPLES + 1;

Native::Error Wrong() {
  return {.Kind = Native::ErrorKind::Invalid, .Message = "CPU calibration result differs"};
}

template <typename Run, typename Verify>
Native::Result<void> Samples(Workload &work, Run run, Verify verify) {
  bool correct = true;
  for (size_t round = 0; round < kRounds; ++round) {
    const auto start = Clock::now();
    run();
    const double ns = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    correct = verify() && correct;
    if (round != 0) {
      Sample sample{};
      sample.WallNs = ns;
      work.Samples.push_back(sample);
    }
  }
  if (!correct) { return std::unexpected(Wrong()); }
  return {};
}

Native::Result<Workload> Fma(uint32_t threads) {
  std::vector<FmaValue> input(CALIBRATION_FMA_ELEMENTS);
  std::vector<FmaValue> output(input.size());
  std::vector<FmaValue> expected(input.size());
  for (uint32_t at = 0; at < input.size(); ++at) {
    input[at] = FmaInput(at);
    expected[at] = FmaReference(input[at]);
  }
  Workload work{};
  work.Name = "cpu_fma_" + std::to_string(threads) + "t";
  work.TimeBasis = "host_wall_ns";
  work.Threads = threads;
  work.Elements = input.size();
  work.Iterations = CALIBRATION_FMA_STEPS;
  work.LogicalOperations = work.Elements * work.Iterations * 4u * 2u;
  work.WorkingSetBytes = input.size() * sizeof(FmaValue) * 2u;
  work.Note = "Four independent FMA lanes, compiler vectorization allowed; not a hardware peak. "
              "Workers split the same total work; creation excluded, barriers included.";
  const auto verify = [&] {
    return std::memcmp(output.data(), expected.data(), output.size() * sizeof(FmaValue)) == 0;
  };
  Native::Result<void> result;
  if (threads == 1) {
    result = Samples(work, [&] { FmaKernel(input, output); }, verify);
  } else {
    std::barrier start(static_cast<ptrdiff_t>(threads) + 1);
    std::barrier finish(static_cast<ptrdiff_t>(threads) + 1);
    const auto worker = [&](size_t index) {
      const size_t first = input.size() * index / threads;
      const size_t last = input.size() * (index + 1) / threads;
      const size_t count = last - first;
      for (size_t round = 0; round < kRounds; ++round) {
        start.arrive_and_wait();
        FmaKernel(std::span(input).subspan(first, count), std::span(output).subspan(first, count));
        finish.arrive_and_wait();
      }
    };
    std::vector<std::jthread> workers;
    workers.reserve(threads);
    for (uint32_t index = 0; index < threads; ++index) { workers.emplace_back(worker, index); }
    result = Samples(
        work,
        [&] {
          start.arrive_and_wait();
          finish.arrive_and_wait();
        },
        verify);
  }
  if (!result) { return std::unexpected(result.error()); }
  work.Checksum = Checksum(std::as_bytes(std::span(output)));
  return work;
}

Native::Result<Workload> Triad(std::pair<size_t, uint32_t> sizeAndPasses) {
  const auto [elements, passes] = sizeAndPasses;
  std::vector<uint32_t> a(elements);
  std::vector<uint32_t> b(elements);
  std::vector<uint32_t> into(elements);
  for (uint32_t at = 0; at < elements; ++at) {
    a[at] = at * CALIBRATION_MIX_MULTIPLIER;
    b[at] = at ^ CALIBRATION_TRIAD_MASK;
  }
  Workload work{};
  work.Name = elements == kSmallTriadElements ? "cpu_triad_768k" : "cpu_triad_96m";
  work.TimeBasis = "host_wall_ns";
  work.Threads = 1;
  work.Elements = elements;
  work.Iterations = passes;
  work.WorkingSetBytes = elements * sizeof(uint32_t) * 3u;
  work.LogicalBytes = work.WorkingSetBytes * passes;
  work.Note =
      "uint32 modulo triad C=A+3*B; logical read/read/write bytes, not physical DRAM traffic. "
      "Opaque kernel calls retain every pass; no LTO.";
  const auto result = Samples(
      work,
      [&] {
        for (uint32_t pass = 0; pass < passes; ++pass) { TriadKernel(a, b, into); }
      },
      [&] {
        for (size_t at = 0; at < elements; ++at) {
          if (into[at] != a[at] + 3u * b[at]) { return false; }
        }
        return true;
      });
  if (!result) { return std::unexpected(result.error()); }
  work.Checksum = Checksum(std::as_bytes(std::span(into)));
  return work;
}

Native::Result<Workload> Chase(uint32_t elements, uint32_t steps) {
  std::vector<uint32_t> next(elements);
  std::vector<uint32_t> order(elements);
  std::ranges::iota(order, 0u);
  uint32_t random = kShuffleSeed;
  for (uint32_t at = elements - 1u; at > 0; --at) {
    random ^= random << 13u;
    random ^= random >> kShuffleRightShift;
    random ^= random << 5u;
    std::swap(order[at], order[random % (at + 1u)]);
  }
  for (uint32_t at = 0; at < elements; ++at) { next[order[at]] = order[(at + 1u) % elements]; }
  const auto zero = static_cast<uint32_t>(std::ranges::find(order, 0u) - order.begin());
  const uint32_t expected = order[(zero + steps % elements) % elements];
  std::vector<uint32_t>().swap(order);
  Workload work{};
  work.Name = elements == kSmallChainElements ? "cpu_chase_64k" : "cpu_chase_64m";
  work.TimeBasis = "host_wall_ns";
  work.Threads = 1;
  work.Elements = elements;
  work.Iterations = steps;
  work.LogicalOperations = steps;
  work.WorkingSetBytes = uint64_t{elements} * sizeof(uint32_t);
  work.Note = "Dependent uint32 loads through one deterministic shuffled cycle; ns/access, "
              "not a DRAM latency claim. Permutation and expected endpoint built before timing.";
  uint32_t endpoint = 0;
  const auto result = Samples(
      work, [&] { endpoint = ChaseKernel(next, steps); }, [&] { return endpoint == expected; });
  if (!result) { return std::unexpected(result.error()); }
  work.Checksum = endpoint;
  return work;
}
}

Native::Result<std::vector<Workload>> CpuWorkloads() {
  std::vector<Workload> result;
  for (const uint32_t threads : {1u, 2u, 4u, 6u}) {
    auto work = Fma(threads);
    if (!work) { return std::unexpected(work.error()); }
    result.push_back(std::move(*work));
  }
  for (const auto [elements, passes] :
       {std::pair{kSmallTriadElements, 256u}, std::pair{kLargeTriadElements, 8u}}) {
    auto work = Triad({elements, passes});
    if (!work) { return std::unexpected(work.error()); }
    result.push_back(std::move(*work));
  }
  for (const auto [elements, steps] :
       {std::pair{kSmallChainElements, 4194304u}, std::pair{kLargeChainElements, 1048576u}}) {
    auto work = Chase(elements, steps);
    if (!work) { return std::unexpected(work.error()); }
    result.push_back(std::move(*work));
  }
  return result;
}
}
