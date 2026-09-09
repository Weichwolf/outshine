#ifndef OUTSHINE_BASE_CURVE_SEGMENTEVALUATION_H
#define OUTSHINE_BASE_CURVE_SEGMENTEVALUATION_H

namespace outshine {
struct Placed;
struct Segment;
[[nodiscard]] Placed AdvanceAlong(const Placed &from, const Segment &along, double byM) noexcept;
}
#endif
