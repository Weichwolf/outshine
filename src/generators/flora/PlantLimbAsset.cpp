#include "PlantLimbAsset.h"
#include "PlantLimbPlan.h"
#include "BinaryValueArchive.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kFormat = 0x000131424d494c50ULL;
constexpr size_t kHeaderBytes = sizeof(uint64_t) + 2 * sizeof(uint32_t);
constexpr size_t kSegmentBytes = (2 + 4 * 3 + 4) * sizeof(double);
constexpr double kLeastSpeedM = 1e-12, kRelativeTolerance = 1e-10, kAbsoluteTolerance = 1e-12;

struct Counts {
  uint32_t Limbs = 0, Segments = 0;
};

bool Consume(size_t &remaining, size_t count, size_t unit) {
  if (count > remaining / unit) { return false; }
  remaining -= count * unit;
  return true;
}

bool Fits(Counts counts, size_t bytes) {
  if (bytes < kHeaderBytes || counts.Limbs == 0 || counts.Segments < counts.Limbs ||
      static_cast<uint64_t>(counts.Segments) >
          static_cast<uint64_t>(counts.Limbs) * (PlantLimbPlan::MostAnchors - 1)) {
    return false;
  }
  size_t remaining = bytes - kHeaderBytes;
  return Consume(remaining, counts.Limbs, 2 * sizeof(uint32_t)) &&
         Consume(remaining, counts.Segments, sizeof(uint32_t) + kSegmentBytes) && remaining == 0;
}

bool Finite(Vec3 value) {
  return std::ranges::all_of(value, [](double component) { return std::isfinite(component); });
}

bool Near(double left, double right) {
  return std::abs(left - right) <= kAbsoluteTolerance + kRelativeTolerance * std::abs(right);
}

bool Near(Vec3 left, Vec3 right) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!Near(left[axis], right[axis])) { return false; }
  }
  return true;
}

double Radius(const std::array<double, 4> &coefficients, double parameter) {
  return ((coefficients[3] * parameter + coefficients[2]) * parameter + coefficients[1]) *
             parameter +
         coefficients[0];
}

bool PositiveRadius(const std::array<double, 4> &r) {
  const double end = Radius(r, 1);
  if (!std::ranges::all_of(r, [](double value) { return std::isfinite(value); }) || !(r[0] > 0) ||
      !(end > 0) || !std::isfinite(end)) {
    return false;
  }
  const auto positive = [&](double t) {
    if (t <= 0 || t >= 1) { return true; }
    const double value = Radius(r, t);
    return value > 0 && std::isfinite(value);
  };
  const double scale = std::max({std::abs(r[1]), std::abs(r[2]), std::abs(r[3])});
  if (scale == 0) { return true; }
  const double a = 3 * (r[3] / scale);
  const double b = 2 * (r[2] / scale);
  const double c = r[1] / scale;
  if (a == 0) { return b == 0 || positive(-c / b); }
  const double discriminant = b * b - 4 * a * c;
  if (discriminant < 0) { return true; }
  const double q = -0.5 * (b + std::copysign(std::sqrt(discriminant), b));
  if (q == 0) { return positive(-b / (2 * a)); }
  return positive(q / a) && positive(c / q);
}
}

class PlantLimbArchive {
public:
  static bool ResidentFits(Counts counts, size_t remaining) {
    return Consume(remaining, counts.Limbs, sizeof(PlantLimbPlan) + sizeof(uint32_t)) &&
           Consume(remaining, counts.Segments, sizeof(PlantLimbPlan::Segment) + sizeof(uint32_t));
  }

  static bool Valid(const PlantLimbPlan &plan) {
    if (plan.Nodes_.size() < 2 || plan.Nodes_.size() > PlantLimbPlan::MostAnchors ||
        plan.Segments_.size() + 1 != plan.Nodes_.size()) {
      return false;
    }
    std::array<uint32_t, PlantLimbPlan::MostAnchors> nodes{};
    std::ranges::copy(plan.Nodes_, nodes.begin());
    const auto selected = std::span(nodes).first(plan.Nodes_.size());
    std::ranges::sort(selected);
    if (std::ranges::adjacent_find(selected) != selected.end()) { return false; }
    const double step = 1.0 / static_cast<double>(plan.Segments_.size());
    for (size_t at = 0; at < plan.Segments_.size(); ++at) {
      const auto &s = plan.Segments_[at];
      const double parameter = static_cast<double>(at) * step;
      const double end = at + 1 == plan.Segments_.size() ? 1 : static_cast<double>(at + 1) * step;
      if (s.Parameter != parameter || s.Span != end - parameter || !PositiveRadius(s.Radius) ||
          !std::ranges::all_of(s.Controls, [](Vec3 control) { return Finite(control); })) {
        return false;
      }
      const Vec3 chord = s.Controls[3] - s.Controls[0];
      const double length = Length(chord);
      if (!(length > 0) || !std::isfinite(length)) { return false; }
      const Vec3 guide = chord * (1 / length);
      for (size_t control = 0; control < 3; ++control) {
        const Vec3 velocity = (s.Controls[control + 1] - s.Controls[control]) * (3 / s.Span);
        if (!Finite(velocity) || !(Dot(velocity, guide) > kLeastSpeedM)) { return false; }
      }
      if (at > 0) {
        const auto &previous = plan.Segments_[at - 1];
        const auto &p = previous.Controls;
        if (p[3] != s.Controls[0] ||
            !Near((p[3] - p[2]) * (1 / previous.Span),
                  (s.Controls[1] - s.Controls[0]) * (1 / s.Span)) ||
            !Near(Radius(previous.Radius, 1), s.Radius[0]) ||
            !Near((previous.Radius[1] + 2 * previous.Radius[2] + 3 * previous.Radius[3]) /
                      previous.Span,
                  s.Radius[1] / s.Span)) {
          return false;
        }
      }
    }
    return true;
  }

  static bool Write(BinaryValueWriter &out, const PlantLimbPlan &plan) {
    if (!Valid(plan) || !out.Array(std::span<const uint32_t>(plan.Nodes_))) { return false; }
    for (const auto &segment : plan.Segments_) {
      if (!out(segment.Parameter, segment.Span)) { return false; }
      for (const Vec3 control : segment.Controls) {
        if (!out(control.Axis)) { return false; }
      }
      if (!out(segment.Radius)) { return false; }
    }
    return true;
  }

  static std::expected<PlantLimbPlan, std::string_view> Read(BinaryValueReader &in,
                                                             uint32_t &remainingSegments) {
    uint32_t nodes = 0;
    if (!in(nodes) || nodes < 2 || nodes > PlantLimbPlan::MostAnchors ||
        nodes - 1 > remainingSegments) {
      return std::unexpected("stored limb node count exceeds prepared capacity");
    }
    PlantLimbPlan plan;
    plan.Nodes_.resize(nodes);
    plan.Segments_.resize(nodes - 1);
    for (auto &node : plan.Nodes_) {
      if (!in(node)) { return std::unexpected("stored limb nodes are incomplete"); }
    }
    for (auto &segment : plan.Segments_) {
      if (!in(segment.Parameter, segment.Span)) {
        return std::unexpected("stored limb parameter interval is incomplete");
      }
      for (Vec3 &control : segment.Controls) {
        if (!in(control.Axis)) { return std::unexpected("stored limb controls are incomplete"); }
      }
      if (!in(segment.Radius)) { return std::unexpected("stored limb radius is incomplete"); }
    }
    if (!Valid(plan)) { return std::unexpected("stored limb has invalid prepared fields"); }
    remainingSegments -= nodes - 1;
    return plan;
  }
};

std::expected<std::vector<uint8_t>, std::string_view>
EncodePlantLimbs(std::span<const PlantLimbPlan> limbs, size_t bytesMost) {
  if (limbs.empty() || limbs.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("bounded nonempty prepared limb set required");
  }
  Counts counts{.Limbs = static_cast<uint32_t>(limbs.size())};
  for (const auto &limb : limbs) {
    if (limb.Nodes().size() < 2 ||
        limb.Nodes().size() - 1 > std::numeric_limits<uint32_t>::max() - counts.Segments) {
      return std::unexpected("prepared limb set has invalid node counts");
    }
    counts.Segments += static_cast<uint32_t>(limb.Nodes().size() - 1);
  }
  size_t remaining = bytesMost;
  if (!Consume(remaining, 1, kHeaderBytes) ||
      !Consume(remaining, counts.Limbs, 2 * sizeof(uint32_t)) ||
      !Consume(remaining, counts.Segments, sizeof(uint32_t) + kSegmentBytes)) {
    return std::unexpected("prepared limb set exceeds encoded budget");
  }
  BinaryValueWriter out(bytesMost);
  out.Out.Reserve(bytesMost - remaining);
  if (!out(kFormat, counts.Limbs, counts.Segments)) {
    return std::unexpected("prepared limb header exceeds encoded budget");
  }
  for (const auto &limb : limbs) {
    if (!PlantLimbArchive::Write(out, limb)) {
      return std::unexpected("prepared limb fields are invalid or exceed budget");
    }
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<std::vector<PlantLimbPlan>, std::string_view>
DecodePlantLimbs(std::span<const uint8_t> bytes, size_t residentBytesMost) {
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  Counts counts;
  if (!in(format, counts.Limbs, counts.Segments) || format != kFormat ||
      !Fits(counts, bytes.size()) || !PlantLimbArchive::ResidentFits(counts, residentBytesMost)) {
    return std::unexpected("stored limb counts, format or byte bounds are invalid");
  }
  std::vector<PlantLimbPlan> limbs;
  limbs.reserve(counts.Limbs);
  for (uint32_t at = 0; at < counts.Limbs; ++at) {
    auto plan = PlantLimbArchive::Read(in, counts.Segments);
    if (!plan) { return std::unexpected(plan.error()); }
    limbs.push_back(std::move(*plan));
  }
  if (counts.Segments != 0 || in.In.Remaining() != 0) {
    return std::unexpected("stored limb set is incomplete or has trailing fields");
  }
  return limbs;
}
}
