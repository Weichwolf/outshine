#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Ephemeris.h"

namespace {

// board:1806: Ephemeris was drawn in the CURRENT class map and named by nothing under test/.
// It decides where the sun is, and the sky stages are green on the strength of it.
//
// The proof is INTERNAL: no almanac is fetched, because the one physical constant the function
// carries -- the obliquity of the ecliptic -- can be recovered from the function itself. Sweep a
// year at the equator, take the highest and lowest solar declination the model produces, and
// what comes out is the tilt of the earth's axis. If the model is wrong about that, it is wrong
// about every shadow it will ever cast.
constexpr double kUnix2024 = 1704067200.0;   // 2024-01-01T00:00:00Z, a leap year
constexpr double kHourS = 3600.0;
constexpr double kDayS = 86400.0;

[[nodiscard]] double SunElAt(double lat, double lon, double utc) {
  float el = 0.0f, az = 0.0f;
  outshine::EarthSunPos(lat, lon, utc, &el, &az);
  return (double)el;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // the sun's declination is its elevation at the equator when it is overhead. Over a year the
  // DAILY MAXIMUM at the equator runs between 90 deg (the equinoxes, sun overhead) and
  // 90 - obliquity (the solstices). So the year's LOWEST daily maximum is the tilt, and the
  // highest is the proof that it reaches overhead at all.
  //
  // A first version of this twin took the global maximum and minimum over the year and called
  // the difference the tilt. That is the equinox measured twice: 1.82 deg came out where 23.44
  // was wanted. The sampling step matters too -- the sun moves 15 deg of hour angle an hour, so
  // an hourly grid misses local noon by up to 1.14 deg of elevation. Minutes.
  double lowestNoon = 90.0, highestNoon = -90.0;
  for (int day = 0; day < 366; ++day) {
    double noonEl = -90.0;
    for (int minute = 0; minute < 1440; ++minute) {
      const double utc = kUnix2024 + (double)day * kDayS + (double)minute * 60.0;
      const double el = SunElAt(0.0, 0.0, utc);
      if (el > noonEl) { noonEl = el; }
    }
    if (noonEl < lowestNoon) { lowestNoon = noonEl; }
    if (noonEl > highestNoon) { highestNoon = noonEl; }
  }
  Note("the highest the noon sun reaches over the equator in 2024", highestNoon, "deg");
  Note("the lowest noon sun of the year there", lowestNoon, "deg");
  Note("the tilt that lowest one implies", 90.0 - lowestNoon, "deg");

  // IAU 2006 gives the mean obliquity at J2000.0 as 23.439279 deg; this header writes 23.439
  // with a linear term. The minute grid leaves 0.25 deg of hour angle, worth about 0.001 deg of
  // elevation at this declination, so the bound is the model's own rounding and not the sweep's.
  CHECK_NEAR(90.0 - lowestNoon, 23.439, 0.01, "deg",
             "**THE TILT THE MODEL CARRIES IS THE EARTH'S OWN**: the year's lowest noon sun at "
             "the equator recovers the obliquity of the ecliptic without an almanac -- a model "
             "wrong about that number is wrong about every shadow it casts (board:1806)");
  // it does not reach exactly 90: the equinox is an INSTANT and local noon on this meridian is
  // another, so the best day of the year catches the sun with a little declination left. The
  // sun's declination moves about 0.4 deg a day near an equinox, so noon can be up to 0.2 deg
  // off it -- and 89.883 says 0.117 deg, inside that.
  CHECK(highestNoon > 89.7 && highestNoon <= 90.0,
        "and at the equinoxes it stands within a fifth of a degree of overhead, which is the "
        "other end of the same sweep and says the declination crosses zero rather than merely "
        "oscillating -- the gap is the equinox instant missing local noon, not an error");

  // polar day and polar night are the obliquity's own consequence, and they are the cheapest
  // check that latitude enters the model at all.
  const double midsummer = kUnix2024 + 172.0 * kDayS;
  int sunUpAtEighty = 0, sunUpAtMinusEighty = 0;
  for (int hour = 0; hour < 24; ++hour) {
    const double utc = midsummer + (double)hour * kHourS;
    sunUpAtEighty += SunElAt(80.0, 0.0, utc) > 0.0 ? 1 : 0;
    sunUpAtMinusEighty += SunElAt(-80.0, 0.0, utc) > 0.0 ? 1 : 0;
  }
  Note("hours the sun is up at 80 N on 21 June", (double)sunUpAtEighty, "of 24");
  Note("hours it is up at 80 S the same day", (double)sunUpAtMinusEighty, "of 24");
  CHECK(sunUpAtEighty == 24 && sunUpAtMinusEighty == 0,
        "**AND THE SAME TILT PUTS THE SUN UP ALL DAY AT ONE POLE AND DOWN ALL DAY AT THE "
        "OTHER**: polar day and polar night fall out of the model rather than being special "
        "cases in it, which is what says latitude reaches the answer");

  // the answer is an ANGLE, and both of its ranges are the caller's contract.
  double worstEl = 0.0, worstAz = 0.0;
  bool azInRange = true, elInRange = true;
  for (long step = 0; step < 4000; ++step) {
    const double utc = kUnix2024 + (double)step * 7919.0;
    const double lat = -85.0 + (double)(step % 35) * 5.0;
    const double lon = -180.0 + (double)(step % 73) * 5.0;
    float el = 0.0f, az = 0.0f;
    outshine::EarthSunPos(lat, lon, utc, &el, &az);
    if (!(el >= -90.0f && el <= 90.0f)) { elInRange = false; worstEl = (double)el; }
    if (!(az >= 0.0f && az < 360.0f)) { azInRange = false; worstAz = (double)az; }
  }
  Note("places and times swept", 4000.0, "samples");
  if (!elInRange) { Note("worst elevation out of range", worstEl, "deg"); }
  if (!azInRange) { Note("worst azimuth out of range", worstAz, "deg"); }
  CHECK(elInRange && azInRange,
        "**AND EVERY ANSWER IS AN ANGLE IN ITS OWN RANGE**: elevation within +/-90 and azimuth "
        "within [0, 360) over 4000 places and times, so a caller can point a light with it "
        "without clamping first");

  // the moon's phase is a fraction and its illumination follows its elongation from the sun.
  double leastPhase = 2.0, mostPhase = -1.0;
  for (long hour = 0; hour < 30 * 24; ++hour) {
    const double utc = kUnix2024 + (double)hour * kHourS;
    float el = 0.0f, az = 0.0f, phase = 0.0f;
    outshine::EarthMoonPos(48.137, 11.575, utc, &el, &az, &phase);
    leastPhase = (double)phase < leastPhase ? (double)phase : leastPhase;
    mostPhase = (double)phase > mostPhase ? (double)phase : mostPhase;
  }
  Note("the darkest the moon gets in a month", leastPhase, "of 1");
  Note("the fullest", mostPhase, "of 1");
  CHECK(leastPhase >= 0.0 && mostPhase <= 1.0 && leastPhase < 0.05 && mostPhase > 0.95,
        "**AND THE MOON RUNS A WHOLE CYCLE IN A MONTH**: the phase is a fraction that reaches "
        "both ends within 30 days, so a scenario asking for moonlight gets a moon that waxes "
        "and wanes rather than a constant");

  // DaylightFactor is the curve the picture reads, and its shape is the contract: clamped at
  // both ends, monotone between them.
  bool monotone = true;
  double before = outshine::DaylightFactor(-30.0);
  for (int deg = -30; deg <= 30; ++deg) {
    const double now = outshine::DaylightFactor((double)deg);
    if (now < before - 1.0e-12) { monotone = false; }
    before = now;
  }
  Note("daylight at 30 deg below the horizon", outshine::DaylightFactor(-30.0), "of 1");
  Note("daylight at the horizon", outshine::DaylightFactor(0.0), "of 1");
  Note("daylight at 30 deg up", outshine::DaylightFactor(30.0), "of 1");
  CHECK(monotone && outshine::DaylightFactor(-30.0) == 0.0 &&
            outshine::DaylightFactor(30.0) == 1.0,
        "and the daylight curve is clamped at both ends and never falls as the sun rises, so a "
        "picture that reads it cannot get brighter as the sun sets");

  // board:1777 named this file's amber: kEphemerisMinYear/kEphemerisMaxYear bound a sphere the
  // engine may not name, and NOTHING reads them. The twin records that, because a constant no
  // code consults is a claim nobody keeps.
  Note("the first year the header declares", (double)outshine::kEphemerisMinYear, "");
  Note("the last", (double)outshine::kEphemerisMaxYear, "");
  const double wayOut = kUnix2024 + 400.0 * 365.25 * kDayS;
  float el = 0.0f, az = 0.0f;
  outshine::EarthSunPos(48.137, 11.575, wayOut, &el, &az);
  std::printf("NOTE 400 years past the declared range it still answers %.3f deg, %.3f deg\n",
              (double)el, (double)az);
  CHECK(std::isfinite((double)el) && std::isfinite((double)az),
        "and outside the years it declares it still returns a number rather than a NaN -- which "
        "is worth knowing, because the bound is documentation and not a refusal (board:1777)");

  Covers("I.86 the solar and lunar position the engine uses is the earth's own: the obliquity "
         "falls out of a year of noon elevations, polar day and polar night fall out of the "
         "obliquity, every answer is an angle in its range, and the moon runs a full cycle in a "
         "month (board:1806)");
  return Report();
}
