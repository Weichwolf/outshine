#ifndef OUTSHINE_MATH_UNITS_H
#define OUTSHINE_MATH_UNITS_H

#include <numbers>

/// @file
/// Numbers every layer shares, sorted by WHAT KIND OF TRUTH they are.
///
/// Mathematics cannot be wrong, the Earth's figures are measurements somebody else made, unit
/// conversions are treaties, and the thresholds at the bottom are the only decisions in the file.
/// They are `constexpr` and public so a caller can READ what the engine computes against -- and
/// cannot change it, which is the point: a scenario declares its world, it does not redefine the
/// foot.
namespace outshine {

/// @name Mathematics
/// Facts about numbers. Nothing here is a decision and nothing here can be wrong.
/// @{

/// Pi.
constexpr double kPi = std::numbers::pi;

/// Degrees in a full turn.
constexpr double kDegPerTurn = 360.0;

/// Degrees in a half turn.
constexpr double kDegPerHalfTurn = 180.0;

/// Multiply degrees by this for radians.
constexpr double kDeg2Rad = kPi / kDegPerHalfTurn;

/// Multiply radians by this for degrees.
constexpr double kRad2Deg = kDegPerHalfTurn / kPi;
/// @}

/// @name The Earth's shape
/// Measured facts about this planet, quoted rather than derived here.
/// @{

/// Metres per degree of longitude AT THE EQUATOR. It shrinks with the cosine of latitude.
constexpr double kMPerDegLon = 111320.0;

/// Metres per degree of latitude, which barely varies -- the meridian is nearly a circle.
constexpr double kMPerDegLat = 111132.0;

/// The equatorial figure, for a caller that means "a degree" without saying which.
constexpr double kMPerDeg = kMPerDegLon;
/// @}

/// @name Units
/// Conversions fixed by treaty or by the SI. A wrong value here would be a typo, never a choice.
/// @{

/// Metres in a kilometre.
constexpr double kMPerKm = 1000.0;

/// Millimetres in a metre.
constexpr double kMmPerM = 1000.0;

/// Milliseconds in a second.
constexpr double kMsPerS = 1000.0;

/// Seconds in a minute.
constexpr double kSPerMin = 60.0;

/// Minutes in an hour.
constexpr double kMinPerHour = 60.0;

/// Seconds in an hour, derived so the two above cannot drift apart from it.
constexpr double kSPerHour = kSPerMin * kMinPerHour;

/// Hours in a day. The CIVIL day; the Earth's rotation is four minutes shorter.
constexpr double kHourPerDay = 24.0;

/// Multiply metres per second by this for kilometres per hour.
constexpr double kMsToKmh = kSPerHour / kMPerKm;

/// Metres in an international foot, fixed by the 1959 agreement.
constexpr double kFtToM = 0.3048;

/// Feet in a metre.
constexpr double kMToFt = 1.0 / kFtToM;

/// Metres in a nautical mile, fixed by the 1929 international definition.
constexpr double kNmToM = 1852.0;

/// Nautical miles in a metre.
constexpr double kMToNm = 1.0 / kNmToM;

/// Multiply knots by this for metres per second.
constexpr double kKtToMs = kNmToM / kSPerHour;

/// Multiply metres per second by this for knots.
constexpr double kMsToKt = 1.0 / kKtToMs;
/// @}

/// @name Thresholds
/// The only DECISIONS in this file, and each one exists because a comparison against zero is
/// wrong in floating point. They say how close counts as equal, and they are stated once so that
/// two places cannot disagree about it.
/// @{

/// Larger than any coordinate this engine will hold, for seeding a minimum.
///
/// Not infinity: this is used as a starting extreme in folds that also SUBTRACT, and infinity
/// minus infinity is a NaN that then poisons everything downstream of it.
constexpr double kBeyondAnyCoordinate = 1.0e30;

/// Shorter than this, in metres, and two places are the same place. A micrometre.
constexpr double kLeastRunM = 1.0e-6;

/// Smaller than this, in radians, and a rotation is no rotation. A nanoradian is a millimetre at
/// a thousand kilometres.
constexpr double kLeastTurnRad = 1.0e-9;

/// Below this, a 2D cross product means the two directions are parallel and no crossing exists.
///
/// It is smaller than the two above because it is a product of two lengths rather than a length,
/// so it carries twice the relative error.
constexpr double kParallelCross = 1.0e-12;
/// @}

} // namespace outshine
#endif
