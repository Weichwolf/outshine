/* FlightBox — FBUnits: the ONE definition of every physical conversion factor and geometric constant the
 * simulator uses. Header-only, constexpr, no translation unit.
 *
 * Why this file exists: the same numbers were re-declared privately in file after file — kMPerDeg six
 * times, kMsToKt five, pi six, kR2D four — and one of them had DRIFTED: knots->m/s was written
 * 0.51444444444 in app/FBMissionBoot.h (the spawn IC) and 0.5144444444 in modules/f16/FBF16Module.cpp
 * (the commanded target speed), so the speed a mission DECLARED and the speed the pilot COMMANDED were
 * converted with different precision. That is exactly the class of bug that multiplies once several
 * units fly at once, and no reader can see it from one file.
 *
 * Values are EXACT definitions where one exists (the nautical mile is defined as exactly 1852 m, the
 * international foot as exactly 0.3048 m) — writing the ratio rather than a truncated decimal is both
 * more accurate and self-documenting. kMsToKt keeps its historical 14-digit literal deliberately: it is
 * consumed by the telemetry columns and by FBMissionMonitor's groundspeed gate, and every site already
 * agreed on it bit-for-bit, so re-deriving it as 3600/1852 would move measured numbers for no gain. */
#ifndef FBUNITS_H
#define FBUNITS_H

namespace FlightBox {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 57.29577951308232;

/* Metres per degree of latitude, spherical approximation. The planar-ENU convention this codebase uses
 * everywhere (see FBGeodesy.h): valid for the tens-of-nautical-miles scales FlightBox actually measures
 * over, not for intercontinental geodesy. */
constexpr double kMPerDeg = 111320.0;

constexpr double kFtToM = 0.3048;              /* exact, by definition of the international foot */
constexpr double kMToFt = 1.0 / kFtToM;
constexpr double kNmToM = 1852.0;              /* exact, by definition of the nautical mile */
constexpr double kMToNm = 1.0 / kNmToM;
constexpr double kKtToMs = kNmToM / 3600.0;    /* exact: 1 kt = 1 nm/h */
constexpr double kMsToKt = 1.9438444924406;    /* historical literal — see the file banner */

} // namespace FlightBox
#endif
