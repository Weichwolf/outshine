/* The ONE definition of every conversion factor the simulator uses — header-only, constexpr.
 * Values are EXACT definitions where one exists: writing the ratio rather than a truncated decimal is
 * both more accurate and self-documenting. doc/core.md, Abschnitt 10.3. */
#ifndef UNITS_H
#define UNITS_H

namespace outshine {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 57.29577951308232;

/* Spherical approximation, the planar-ENU convention: valid for the tens-of-nautical-miles scales
 * outshine measures over, not for intercontinental geodesy. */
constexpr double kMPerDeg = 111320.0;

constexpr double kFtToM = 0.3048;              /* exact, by definition of the international foot */
constexpr double kMToFt = 1.0 / kFtToM;
constexpr double kNmToM = 1852.0;              /* exact, by definition of the nautical mile */
constexpr double kMToNm = 1.0 / kNmToM;
constexpr double kKtToMs = kNmToM / 3600.0;    /* exact: 1 kt = 1 nm/h */
constexpr double kMsToKt = 1.9438444924406;    /* historical 14-digit literal, deliberately kept: every
                                                * site already agreed on it bit-for-bit */

} // namespace outshine
#endif
