#ifndef UNITS_H
#define UNITS_H

namespace outshine {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 57.29577951308232;

constexpr double kMPerDeg = 111320.0;

constexpr double kFtToM = 0.3048;
constexpr double kMToFt = 1.0 / kFtToM;
constexpr double kNmToM = 1852.0;
constexpr double kMToNm = 1.0 / kNmToM;
constexpr double kKtToMs = kNmToM / 3600.0;
constexpr double kMsToKt = 1.9438444924406;

}
#endif
