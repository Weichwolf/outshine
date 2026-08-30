#ifndef OUTSHINE_BASE_MATH_UNITS_H
#define OUTSHINE_BASE_MATH_UNITS_H

#include <numbers>

namespace outshine {

constexpr double kPi = std::numbers::pi;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 180.0 / kPi;

constexpr double kMPerDeg = 111320.0;

constexpr double kFtToM = 0.3048;
constexpr double kMToFt = 1.0 / kFtToM;
constexpr double kNmToM = 1852.0;
constexpr double kMToNm = 1.0 / kNmToM;
constexpr double kKtToMs = kNmToM / 3600.0;
constexpr double kMsToKt = 1.0 / kKtToMs;

}
#endif
