/* Calendar <-> Unix seconds, computed rather than looked up: mktime/timegm read the HOST time zone and
 * the host's leap-second view, so the same mission file would mean a different sky in Zurich and in a
 * container. Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms" (public domain,
 * https://howardhinnant.github.io/date_algorithms.html), proleptic Gregorian, no leap seconds. */
#ifndef CIVILTIME_H
#define CIVILTIME_H

#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace outshine {

/* Days since 1970-01-01 for a proleptic-Gregorian y-m-d; m in [1,12], d in [1,31]. */
constexpr int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned mp = (m > 2 ? m - 3u : m + 9u);
  const unsigned doy = (153u * mp + 2u) / 5u + d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

constexpr void CivilFromDays(int64_t z, int64_t &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
  const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
  const unsigned mp = (5u * doy + 2u) / 153u;
  d = doy - (153u * mp + 2u) / 5u + 1u;
  m = (mp < 10u) ? mp + 3u : mp - 9u;
  y = static_cast<int64_t>(yoe) + era * 400 + (m <= 2);
}

constexpr bool IsLeapYear(int64_t y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

constexpr unsigned DaysInMonth(int64_t y, unsigned m) {
  constexpr unsigned kLen[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && IsLeapYear(y)) ? 29u : kLen[m];
}

/* Exactly YYYY-MM-DDThh:mm:ssZ and nothing else — no offset spelling, no fractional seconds, no
 * omitted seconds. A best-effort read here would let a typo silently mean another sky. */
inline bool ParseIsoUtc(const char *s, int64_t &outUnixS) {
  if (!s) return false;
  size_t n = 0;
  while (s[n]) { if (++n > 20) return false; }
  if (n != 20) return false;
  const char sep[20] = {0, 0, 0, 0, '-', 0, 0, '-', 0, 0, 'T', 0, 0, ':', 0, 0, ':', 0, 0, 'Z'};
  int v[20];
  for (int i = 0; i < 20; i++) {
    if (sep[i]) { if (s[i] != sep[i]) return false; v[i] = 0; continue; }
    if (s[i] < '0' || s[i] > '9') return false;
    v[i] = s[i] - '0';
  }
  const int64_t year = v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3];
  const unsigned mon = static_cast<unsigned>(v[5] * 10 + v[6]);
  const unsigned day = static_cast<unsigned>(v[8] * 10 + v[9]);
  const int hour = v[11] * 10 + v[12], min = v[14] * 10 + v[15], sec = v[17] * 10 + v[18];
  if (mon < 1 || mon > 12) return false;
  if (day < 1 || day > DaysInMonth(year, mon)) return false;
  /* 60 is a leap second, and this calendar has none — accepting it would invent one. */
  if (hour > 23 || min > 59 || sec > 59) return false;
  outUnixS = DaysFromCivil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec;
  return true;
}

/* The inverse, for logs. Buffer must hold 21 bytes. gmtime_r would do for a Unix epoch, but then the
 * write and the read of the same instant would run through two different calendars. */
inline const char *FormatIsoUtc(int64_t unixS, char *buf, size_t n) {
  int64_t days = unixS / 86400;
  int64_t rem = unixS - days * 86400;
  if (rem < 0) { rem += 86400; days -= 1; }
  int64_t y; unsigned m, d;
  CivilFromDays(days, y, m, d);
  snprintf(buf, n, "%04lld-%02u-%02uT%02u:%02u:%02uZ", static_cast<long long>(y), m, d,
           static_cast<unsigned>(rem / 3600), static_cast<unsigned>((rem / 60) % 60),
           static_cast<unsigned>(rem % 60));
  return buf;
}

} // namespace outshine
#endif
