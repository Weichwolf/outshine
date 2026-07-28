/* The mission's `time` declaration turned into the run's clock — the sibling of FBWeatherBoot.h, and
 * the ONE place the precedence between a file and a client flag is decided.
 *
 * THE RULE, one sentence, every client: a mission that DECLARES a clock has it, on all three clients
 * identically; a mission that declares none leaves the client its own path (fb-gym: no clock at all,
 * native/wasm: --utc / FB_SIM_UTC or the host wall clock); and a mission that declares one WHILE the
 * client flag is set is a BOOT ERROR, not a precedence.
 *
 * That last row is where the clock deliberately differs from the weather: `wx` has no flag, so no
 * collision can exist, while gpu_native must keep `--utc` because it also runs with no mission at all.
 * Keeping the flag and making the collision fatal costs one comparison and buys the same guarantee
 * deleting it would: a measurement can never silently run under a sky the file did not declare.
 * doc/clients/clients.md, "Clock defaults per client". */
#ifndef FBCLOCKBOOT_H
#define FBCLOCKBOOT_H

#include <string>
#include "FBCivilTime.h"
#include "FBMissionFile.h"

namespace FlightBox::Missions {

/* `Have == false` is NOT "epoch": it means no clock exists and no channel is touched. */
struct FBMissionClock {
  bool    Have = false;
  int64_t T0S = 0;   /* UTC Unix seconds at simT = 0 */

  double At(double simT) const { return (double)T0S + simT; }   /* the clock ADVANCES with sim time */
};

/* False = the client flag contradicts the file; *err then says so in the terms the caller must print. */
inline bool FBResolveMissionClock(const FBMission &mission, bool clientClockOverride,
                                  FBMissionClock &out, std::string *err) {
  if (err) err->clear();
  out = FBMissionClock{};
  if (mission.HaveTime && clientClockOverride) {
    char iso[21];
    if (err)
      *err = std::string("mission declares 'time ") + FBFormatIsoUtc(mission.UtcT0S, iso, sizeof iso) +
             "' and a client clock override (--utc / FB_SIM_UTC) is set — drop one of the two";
    return false;
  }
  if (mission.HaveTime) { out.Have = true; out.T0S = mission.UtcT0S; }
  return true;
}

} // namespace FlightBox::Missions
#endif
