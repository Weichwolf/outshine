#ifndef OUTSHINE_EXPERIMENTS_PROJECTION_ENVELOPE_H
#define OUTSHINE_EXPERIMENTS_PROJECTION_ENVELOPE_H
#include "Input.h"
#include <array>
#include <cstdint>
#include <expected>

namespace Projection {
struct Interval {
  double Lower = 0;
  double Upper = 0;
};

struct EnvelopeCosts {
  uint64_t Inputs = 0;
  uint64_t Products = 0;
  uint64_t Sums = 0;
  uint64_t OutwardSteps = 0;
  uint64_t Divisions = 0;
};

struct Envelope {
  std::array<Interval, 4> Clip;
  EnvelopeCosts Work;
};

enum class EnvelopeUnknown {
  CpuArithmetic,
  InputSubnormal,
  InputNonfinite,
  OverflowRisk,
  ClipCrossing
};

[[nodiscard]] std::expected<Envelope, EnvelopeUnknown> Enclose(const Input &input) noexcept;
[[nodiscard]] std::expected<std::array<Interval, 3>, EnvelopeUnknown>
Divide(const Envelope &envelope) noexcept;
}
#endif
