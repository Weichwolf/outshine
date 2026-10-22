#include "UncertainPatch.h"
#include "FixedPatch.h"
#include "vertex_projection/Envelope.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>

namespace FixedCoverage {
namespace {
using Wide = __uint128_t;
using Point = std::array<int64_t, 2>;
using Approximate = std::array<double, 2>;
constexpr int64_t Scale = static_cast<int64_t>(uint64_t{1} << 16U);
constexpr int64_t Maximum = static_cast<int64_t>(uint64_t{1} << 29U);
constexpr uint64_t DoubleFraction = (uint64_t{1} << 52) - 1;
constexpr uint64_t DoubleExponent = (uint64_t{1} << 11) - 1;
constexpr uint64_t DoubleOne = uint64_t{0x3ff} << 52;
constexpr uint32_t FloatOne = 0x3f800000;
constexpr uint32_t DoubleSignBit = 63;
constexpr int MaximumRatioBits = 31;
constexpr uint32_t DoubleFractionBits = 52;
constexpr uint32_t FloatFractionBits = 23;
constexpr int DoubleBias = 1023;
constexpr int FloatBias = 127;
static_assert(sizeof(Wide) == 16);
static_assert(sizeof(double) == sizeof(uint64_t) && std::bit_cast<uint64_t>(1.0) == DoubleOne);
static_assert(sizeof(float) == sizeof(uint32_t) && std::bit_cast<uint32_t>(1.f) == FloatOne);
static_assert(8 * Maximum * Maximum < std::numeric_limits<int64_t>::max());

struct Binary {
  uint64_t Mantissa = 0;
  int Power = 0;
  bool Negative = false;
};

Binary Decode(double value) {
  const auto bits = std::bit_cast<uint64_t>(value);
  const auto exponent = (bits >> DoubleFractionBits) & DoubleExponent;
  return {.Mantissa =
              (bits & DoubleFraction) | (exponent != 0 ? uint64_t{1} << DoubleFractionBits : 0),
          .Power = static_cast<int>(exponent == 0 ? 1 : exponent) - DoubleBias -
                   static_cast<int>(DoubleFractionBits),
          .Negative = (bits >> DoubleSignBit) != 0};
}

int Width(Wide value) {
  const auto high = static_cast<uint64_t>(value >> 64u);
  return high != 0 ? 64 + std::bit_width(high) : std::bit_width(static_cast<uint64_t>(value));
}

int Sign(int64_t value) {
  if (value == 0) { return 0; }
  return value < 0 ? -1 : 1;
}

int Compare(Wide a, int ae, Wide b, int be) {
  if (a == b && a == 0) { return 0; }
  if (a == 0) { return -1; }
  if (b == 0) { return 1; }
  const int at = Width(a) + ae;
  const int bt = Width(b) + be;
  if (at != bt) { return at < bt ? -1 : 1; }
  if (ae < be) {
    b <<= static_cast<unsigned>(be - ae);
  } else {
    a <<= static_cast<unsigned>(ae - be);
  }
  if (a == b) { return 0; }
  return a < b ? -1 : 1;
}

struct IntegerRange {
  int64_t Low, High;
};

std::expected<IntegerRange, Unknown>
Ratio(const std::array<double, 2> &fraction, uint32_t dimension, bool negate) {
  const auto a = Decode(fraction[0]);
  const auto b = Decode(fraction[1]);
  if (a.Mantissa == 0) { return IntegerRange{.Low = 0, .High = 0}; }
  Wide n = Wide{a.Mantissa} * (uint64_t{dimension} * static_cast<uint64_t>(Scale) / 2);
  Wide d = b.Mantissa;
  const int shift = a.Power - b.Power;
  if (Width(n) + shift > Width(d) + MaximumRatioBits) {
    return std::unexpected(Unknown::Quantization);
  }
  bool remainder = true;
  Wide quotient = 0;
  if (shift >= 0) {
    n <<= static_cast<unsigned>(shift);
    quotient = n / d;
    remainder = n % d != 0;
  } else if (Width(d) - shift <= Width(n)) {
    d <<= static_cast<unsigned>(-shift);
    quotient = n / d;
    remainder = n % d != 0;
  }
  if (quotient > static_cast<uint64_t>(2 * Maximum)) {
    return std::unexpected(Unknown::Quantization);
  }
  const auto low = static_cast<int64_t>(quotient);
  const auto high = low + (remainder ? 1 : 0);
  return a.Negative != negate ? IntegerRange{.Low = -high, .High = -low}
                              : IntegerRange{.Low = low, .High = high};
}

struct Rectangle {
  Point Low;
  Point High;
};

bool FiniteNormalOrZero(double value) {
  const auto bits = std::bit_cast<uint64_t>(value);
  const auto exponent = (bits >> DoubleFractionBits) & DoubleExponent;
  return exponent != DoubleExponent && (exponent != 0 || (bits & DoubleFraction) == 0);
}

std::expected<void, Unknown> ValidateClip(const std::array<Projection::Interval, 4> &clip) {
  if (!std::ranges::all_of(clip, [](const auto &range) {
        return FiniteNormalOrZero(range.Lower) && FiniteNormalOrZero(range.Upper) &&
               range.Lower <= range.Upper;
      })) {
    return std::unexpected(Unknown::Nonfinite);
  }
  if (clip[3].Lower <= 0 || clip[2].Lower < 0 || clip[2].Upper > clip[3].Lower) {
    return std::unexpected(Unknown::ClipCrossing);
  }
  return {};
}

std::expected<std::array<Rectangle, 4>, Unknown>
Rectangles(const UncertainQuad &source, uint32_t width, uint32_t height) {
  std::array<Rectangle, 4> result;
  for (size_t i = 0; i < 4; ++i) {
    const auto &clip = source.Clip[i];
    const auto valid = ValidateClip(clip);
    if (!valid) { return std::unexpected(valid.error()); }
    for (size_t axis = 0; axis < 2; ++axis) {
      const auto dimension = axis == 0 ? width : height;
      const auto centre = static_cast<int64_t>(dimension) * Scale / 2;
      int64_t low = Maximum;
      int64_t high = -Maximum;
      for (const double value : {clip[axis].Lower, clip[axis].Upper}) {
        for (const double divisor : {clip[3].Lower, clip[3].Upper}) {
          const auto range = Ratio({value, divisor}, dimension, axis == 1);
          if (!range) { return std::unexpected(range.error()); }
          low = std::min(low, centre + range->Low);
          high = std::max(high, centre + range->High);
        }
      }
      if (low < -Maximum || high > Maximum) { return std::unexpected(Unknown::Quantization); }
      result[i].Low[axis] = low;
      result[i].High[axis] = high;
    }
  }
  return result;
}

Point Corner(const Rectangle &box, size_t n) {
  return {(n & 1u) != 0 ? box.High[0] : box.Low[0], (n & 2u) != 0 ? box.High[1] : box.Low[1]};
}

int64_t Edge(const Point &a, const Point &b, const Point &p) {
  return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]);
}

bool Facing(const std::array<Rectangle, 4> &rectangles, int sign) {
  for (size_t i = 0; i < 4; ++i) {
    for (size_t a = 0; a < 4; ++a) {
      for (size_t b = 0; b < 4; ++b) {
        for (size_t c = 0; c < 4; ++c) {
          const auto edge = Edge(Corner(rectangles[i], a),
                                 Corner(rectangles[(i + 1) % 4], b),
                                 Corner(rectangles[(i + 2) % 4], c));
          if (Sign(edge) != sign) { return false; }
        }
      }
    }
  }
  return true;
}

struct Polygon {
  std::array<Approximate, 16> Points{};
  size_t Count = 0;
};

bool Push(Polygon &polygon, const Approximate &point) {
  if (polygon.Count > 0 && polygon.Points[polygon.Count - 1] == point) { return true; }
  if (polygon.Count == polygon.Points.size()) { return false; }
  polygon.Points[polygon.Count++] = point;
  return true;
}

bool Clip(Polygon &polygon, size_t axis, double bound, bool upper) {
  if (polygon.Count == 0) { return false; }
  Polygon result;
  auto previous = polygon.Points[polygon.Count - 1];
  const auto distance = [&](const Approximate &p) {
    return upper ? bound - p[axis] : p[axis] - bound;
  };
  double before = distance(previous);
  for (size_t i = 0; i < polygon.Count; ++i) {
    const auto &point = polygon.Points[i];
    const double now = distance(point);
    if ((now >= 0) != (before >= 0)) {
      const double t = before / (before - now);
      Approximate hit{previous[0] + (point[0] - previous[0]) * t,
                      previous[1] + (point[1] - previous[1]) * t};
      if (now == 0) { hit = point; }
      if (before == 0) { hit = previous; }
      hit[axis] = bound;
      if (!Push(result, hit)) { return false; }
    }
    if (now >= 0 && !Push(result, point)) { return false; }
    previous = point;
    before = now;
  }
  if (result.Count > 1 && result.Points[0] == result.Points[result.Count - 1]) { --result.Count; }
  polygon = result;
  return polygon.Count >= 3;
}

bool Inside(const std::array<Rectangle, 4> &rectangles, const Point &point) {
  for (size_t i = 0; i < 4; ++i) {
    for (size_t a = 0; a < 4; ++a) {
      for (size_t b = 0; b < 4; ++b) {
        if (Edge(Corner(rectangles[i], a), Corner(rectangles[(i + 1) % 4], b), point) < 0) {
          return false;
        }
      }
    }
  }
  return true;
}

bool Convex(const FixedPatch &patch) {
  int64_t area = 0;
  for (size_t i = 0; i < patch.Meta[0]; ++i) {
    const Point a{patch.Corners[i][0], patch.Corners[i][1]};
    const Point b{patch.Corners[(i + 1) % patch.Meta[0]][0],
                  patch.Corners[(i + 1) % patch.Meta[0]][1]};
    area += a[0] * b[1] - a[1] * b[0];
    for (size_t j = 0; j < patch.Meta[0]; ++j) {
      if (Edge(a, b, {patch.Corners[j][0], patch.Corners[j][1]}) < 0) { return false; }
    }
  }
  return area > 0;
}

std::expected<float, Unknown> Depth(const UncertainQuad &source) {
  float result = 1;
  for (const auto &clip : source.Clip) {
    auto value = static_cast<float>(clip[2].Lower / clip[3].Upper);
    if (!(value > 0) || !std::isnormal(value)) { return std::unexpected(Unknown::NoDepth); }
    const auto z = Decode(clip[2].Lower);
    const auto w = Decode(clip[3].Upper);
    const auto compare = [&](float v) {
      const auto bits = std::bit_cast<uint32_t>(v);
      const uint64_t mantissa =
          (bits & ((1u << FloatFractionBits) - 1)) | (uint64_t{1} << FloatFractionBits);
      const auto exponent = static_cast<int>((bits >> FloatFractionBits) & 255u) - FloatBias -
                            static_cast<int>(FloatFractionBits);
      return Compare(Wide{mantissa} * w.Mantissa, exponent + w.Power, z.Mantissa, z.Power);
    };
    if (compare(value) > 0) { value = std::bit_cast<float>(std::bit_cast<uint32_t>(value) - 1u); }
    if (!std::isnormal(value) || compare(value) > 0) { return std::unexpected(Unknown::NoDepth); }
    result = std::min(result, value);
  }
  return result;
}

std::optional<FixedPatch> Candidate(const Polygon &polygon,
                                    const Approximate &centre,
                                    const std::array<Rectangle, 4> &rectangles,
                                    const std::array<uint32_t, 3> &profile,
                                    double contraction) {
  const auto [width, height, grid] = profile;
  FixedPatch patch;
  patch.Meta = {0, width, height, grid};
  for (size_t i = 0; i < polygon.Count; ++i) {
    Point point{};
    for (size_t axis = 0; axis < 2; ++axis) {
      const double p =
          polygon.Points[i][axis] + (centre[axis] - polygon.Points[i][axis]) * contraction;
      point[axis] = static_cast<int64_t>(std::floor(
          std::clamp(p, 0.0, axis == 0 ? static_cast<double>(width) : height) * grid + .5));
    }
    if (patch.Meta[0] > 0 && patch.Corners[patch.Meta[0] - 1][0] == point[0] &&
        patch.Corners[patch.Meta[0] - 1][1] == point[1]) {
      continue;
    }
    patch.Corners[patch.Meta[0]++] = {
        static_cast<int32_t>(point[0]), static_cast<int32_t>(point[1]), 0, 0};
  }
  if (patch.Meta[0] > 1 && patch.Corners[0] == patch.Corners[patch.Meta[0] - 1]) {
    --patch.Meta[0];
  }
  if (patch.Meta[0] < 3 || !Convex(patch)) { return std::nullopt; }
  bool certified = true;
  for (size_t i = 0; i < patch.Meta[0]; ++i) {
    certified = certified && Inside(rectangles,
                                    {int64_t{patch.Corners[i][0]} * (Scale / grid),
                                     int64_t{patch.Corners[i][1]} * (Scale / grid)});
  }
  return certified ? std::optional(patch) : std::nullopt;
}

std::expected<FixedPatch, Unknown>
Quantize(std::array<Rectangle, 4> rectangles, uint32_t width, uint32_t height) {
  Polygon polygon;
  for (const auto &box : rectangles) {
    if (!Push(polygon,
              {static_cast<double>(box.Low[0] + box.High[0]) / (2 * Scale),
               static_cast<double>(box.Low[1] + box.High[1]) / (2 * Scale)})) {
      return std::unexpected(Unknown::Quantization);
    }
  }
  if (!Clip(polygon, 0, 0, false) || !Clip(polygon, 0, width, true) ||
      !Clip(polygon, 1, 0, false) || !Clip(polygon, 1, height, true) ||
      polygon.Count > MaximumCorners) {
    return std::unexpected(Unknown::Outside);
  }
  Approximate centre{};
  for (size_t i = 0; i < polygon.Count; ++i) {
    centre[0] += polygon.Points[i][0] / static_cast<double>(polygon.Count);
    centre[1] += polygon.Points[i][1] / static_cast<double>(polygon.Count);
  }
  uint32_t grid = PreferredScale;
  while (std::max(width, height) * grid > MaximumCoordinate) { grid /= 2; }
  double maximumMotion = 0;
  for (size_t i = 0; i < polygon.Count; ++i) {
    for (size_t axis = 0; axis < 2; ++axis) {
      maximumMotion = std::max(maximumMotion, std::abs(centre[axis] - polygon.Points[i][axis]));
    }
  }
  const auto candidate = [&](double contraction) {
    return Candidate(polygon, centre, rectangles, {width, height, grid}, contraction);
  };
  constexpr int MaximumShift = 20;
  double lower = 0;
  for (int shift = MaximumShift; shift >= 0; --shift) {
    double upper = std::ldexp(1.0, -shift);
    auto best = candidate(upper);
    if (!best) {
      lower = upper;
      continue;
    }
    for (int refinement = 0;
         refinement < MaximumShift && (upper - lower) * maximumMotion * grid > 1;
         ++refinement) {
      const double middle = (lower + upper) * .5;
      const auto refined = candidate(middle);
      if (refined) {
        best = refined;
        upper = middle;
      } else {
        lower = middle;
      }
    }
    return *best;
  }
  return std::unexpected(Unknown::Quantization);
}
}

std::expected<FixedPatch, Unknown>
PrepareInteger(const UncertainQuad &source, uint32_t width, uint32_t height) {
  if (!source.Opaque) { return std::unexpected(Unknown::NonOpaque); }
  if (!source.Complete) { return std::unexpected(Unknown::Incomplete); }
  if (width == 0 || height == 0 || std::max(width, height) > Maximum / Scale ||
      (source.FrontWindowOrientation != 1 && source.FrontWindowOrientation != -1)) {
    return std::unexpected(Unknown::InvalidProfile);
  }
  auto rectangles = Rectangles(source, width, height);
  if (!rectangles) { return std::unexpected(rectangles.error()); }
  if (!Facing(*rectangles, source.FrontWindowOrientation)) {
    return std::unexpected(Unknown::UncertainFacing);
  }
  if (source.FrontWindowOrientation < 0) { std::ranges::reverse(*rectangles); }
  auto packet = Quantize(*rectangles, width, height);
  if (!packet) { return std::unexpected(packet.error()); }
  const auto depth = Depth(source);
  if (!depth) { return std::unexpected(depth.error()); }
  packet->Depth[0] = *depth;
  return packet;
}
}
