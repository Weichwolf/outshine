#ifndef OUTSHINE_WORLD_SKY_CIVILTIME_H
#define OUTSHINE_WORLD_SKY_CIVILTIME_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace outshine {

constexpr int64_t kYearsPerEra = 400;
constexpr int64_t kDaysPerEra = 146097;
constexpr int64_t kLastDayOfEra = kDaysPerEra - 1;
constexpr int64_t kEpochShiftDays = 719468;
constexpr unsigned kDaysPerCommonYear = 365u;
constexpr unsigned kLeapEvery = 4u;
constexpr unsigned kNotLeapEvery = 100u;
constexpr unsigned kDaysPerLeapCycle = kDaysPerCommonYear * kLeapEvery + 1u;
constexpr unsigned kDaysPerCentury =
    kDaysPerCommonYear * kNotLeapEvery + kNotLeapEvery / kLeapEvery - 1u;
constexpr unsigned kMonthsShifted = 3u;
constexpr unsigned kMonthsPerYear = 12u;
constexpr unsigned kFiveMonthDays = 153u;
constexpr unsigned kFiveMonths = 5u;
constexpr unsigned kFebruary = 2u;
constexpr unsigned kFebruaryLeapDays = 29u;

constexpr int64_t kSecondsPerDay = 86400;
constexpr int64_t kSecondsPerHour = 3600;
constexpr int64_t kSecondsPerMinute = 60;
constexpr int64_t kMinutesPerHour = 60;
constexpr int kHourMost = 23;
constexpr int kMinuteMost = 59;
constexpr int kSecondMost = 59;
constexpr size_t kIsoLength = 20;

constexpr int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= static_cast<int64_t>(m <= kFebruary);
  const int64_t era = (y >= 0 ? y : y - (kYearsPerEra - 1)) / kYearsPerEra;
  const auto yoe = static_cast<unsigned>(y - era * kYearsPerEra);
  const unsigned mp = (m > kFebruary ? m - kMonthsShifted : m + kMonthsPerYear - kMonthsShifted);
  const unsigned doy = (kFiveMonthDays * mp + kFebruary) / kFiveMonths + d - 1u;
  const unsigned doe = yoe * kDaysPerCommonYear + yoe / kLeapEvery - yoe / kNotLeapEvery + doy;
  return era * kDaysPerEra + static_cast<int64_t>(doe) - kEpochShiftDays;
}

constexpr void CivilFromDays(int64_t z, int64_t &y, unsigned &m, unsigned &d) {
  z += kEpochShiftDays;
  const int64_t era = (z >= 0 ? z : z - kLastDayOfEra) / kDaysPerEra;
  const auto doe = static_cast<unsigned>(z - era * kDaysPerEra);
  const unsigned yoe = (doe - doe / kDaysPerLeapCycle + doe / kDaysPerCentury -
                        doe / static_cast<unsigned>(kLastDayOfEra)) /
                       kDaysPerCommonYear;
  const unsigned doy = doe - (kDaysPerCommonYear * yoe + yoe / kLeapEvery - yoe / kNotLeapEvery);
  const unsigned mp = (kFiveMonths * doy + kFebruary) / kFiveMonthDays;
  d = doy - (kFiveMonthDays * mp + kFebruary) / kFiveMonths + 1u;
  m = (mp < kMonthsPerYear - kFebruary) ? mp + kMonthsShifted
                                        : mp - (kMonthsPerYear - kMonthsShifted);
  y = static_cast<int64_t>(yoe) + era * kYearsPerEra + static_cast<int64_t>(m <= kFebruary);
}

[[nodiscard]] constexpr bool IsLeapYear(int64_t y) {
  return (y % kLeapEvery == 0 && y % kNotLeapEvery != 0) || y % kYearsPerEra == 0;
}

constexpr unsigned DaysInMonth(int64_t y, unsigned m) {
  constexpr std::array<unsigned, 13> kLen = {{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
  return (m == kFebruary && IsLeapYear(y)) ? kFebruaryLeapDays : kLen[m];
}

[[nodiscard]] inline bool ParseIsoUtc(const char *s, int64_t &outUnixS) {
  if (s == nullptr) { return false; }
  size_t n = 0;
  while (s[n] != 0) {
    if (++n > kIsoLength) { return false; }
  }
  if (n != kIsoLength) { return false; }
  const std::array<char, kIsoLength> sep = {
      {0, 0, 0, 0, '-', 0, 0, '-', 0, 0, 'T', 0, 0, ':', 0, 0, ':', 0, 0, 'Z'}};
  std::array<int, kIsoLength> v{};
  for (size_t i = 0; i < kIsoLength; i++) {
    if (sep[i] != 0) {
      if (s[i] != sep[i]) { return false; }
      v[i] = 0;
      continue;
    }
    if (s[i] < '0' || s[i] > '9') { return false; }
    v[i] = s[i] - '0';
  }
  const int64_t year = v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3];
  const auto mon = static_cast<unsigned>(v[5] * 10 + v[6]);
  const auto day = static_cast<unsigned>(v[8] * 10 + v[9]);
  const int hour = v[11] * 10 + v[12];
  const int min = v[14] * 10 + v[15];
  const int sec = v[17] * 10 + v[18];
  if (mon < 1 || mon > 12) { return false; }
  if (day < 1 || day > DaysInMonth(year, mon)) { return false; }

  if (hour > kHourMost || min > kMinuteMost || sec > kSecondMost) { return false; }
  outUnixS = DaysFromCivil(year, mon, day) * kSecondsPerDay + hour * kSecondsPerHour +
             min * kSecondsPerMinute + sec;
  return true;
}

inline const char *FormatIsoUtc(int64_t unixS, char *buf, size_t n) {
  int64_t days = unixS / kSecondsPerDay;
  int64_t rem = unixS - days * kSecondsPerDay;
  if (rem < 0) {
    rem += kSecondsPerDay;
    days -= 1;
  }
  int64_t y;
  unsigned m;
  unsigned d;
  CivilFromDays(days, y, m, d);
  snprintf(buf,
           n,
           "%04lld-%02u-%02uT%02u:%02u:%02uZ",
           static_cast<long long>(y),
           m,
           d,
           static_cast<unsigned>(rem / kSecondsPerHour),
           static_cast<unsigned>((rem / kSecondsPerMinute) % kMinutesPerHour),
           static_cast<unsigned>(rem % kSecondsPerMinute));
  return buf;
}

} // namespace outshine
#endif
