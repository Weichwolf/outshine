#ifndef OUTSHINE_MATH_UNITS_H
#define OUTSHINE_MATH_UNITS_H

#include <numbers>

namespace outshine {

constexpr double kPi = std::numbers::pi;

constexpr double kDegPerTurn = 360.0;

constexpr double kDegPerHalfTurn = 180.0;

constexpr double kDeg2Rad = kPi / kDegPerHalfTurn;

constexpr double kRad2Deg = kDegPerHalfTurn / kPi;

constexpr double kMPerDegLon = 111320.0;

constexpr double kMPerDegLat = 111132.0;

constexpr double kWgs84A = 6378137.0;

constexpr double kWgs84F = 1.0 / 298.257223563;

constexpr double kWgs84E2 = kWgs84F * (2.0 - kWgs84F);

constexpr double kMPerKm = 1000.0;

constexpr double kMmPerM = 1000.0;

constexpr double kMsPerS = 1000.0;

constexpr double kSPerMin = 60.0;

constexpr double kMinPerHour = 60.0;

constexpr double kSPerHour = kSPerMin * kMinPerHour;

constexpr double kHourPerDay = 24.0;

constexpr double kMsToKmh = kSPerHour / kMPerKm;

constexpr double kFtToM = 0.3048;

constexpr double kMToFt = 1.0 / kFtToM;

constexpr double kNmToM = 1852.0;

constexpr double kMToNm = 1.0 / kNmToM;

constexpr double kKtToMs = kNmToM / kSPerHour;

constexpr double kMsToKt = 1.0 / kKtToMs;

constexpr double kBeyondAnyCoordinate = 1.0e30;

constexpr double kLeastRunM = 1.0e-6;

constexpr double kLeastTurnRad = 1.0e-9;

constexpr double kParallelCross = 1.0e-12;

}
#endif
