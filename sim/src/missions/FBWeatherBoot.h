/* The mission's `wx` declaration turned into a live provider — the weather counterpart of
 * FBMissionBoot's spawn, and in app/ for the same reason: only a client knows where its files are.
 *
 * THE PRECEDENCE RULE, one sentence, every client: a mission that DECLARES weather always wins; a
 * mission that does not gets the client's own default (fb-gym and the native oracle: calm, so a
 * measurement is reproducible — the browser: live /wx). doc/missions/weather.md, "Wetter". */
#ifndef FBWEATHERBOOT_H
#define FBWEATHERBOOT_H

#include <memory>
#include <string>
#include "FBCalmWeather.h"
#include "FBConstantWindWeather.h"
#include "FBFixedWeather.h"
#include "FBMissionFile.h"
#include "FBModelRoots.h"

namespace FlightBox::Missions {

/* Null iff the mission declared a fixture this client cannot load; *err then says which path failed.
 * A mission with no `wx` line returns null WITHOUT an error — that is the caller's default to fill. */
inline std::unique_ptr<FBWeatherProvider> FBMakeMissionWeather(const FBMission &mission,
                                                               const FBModelRoots &roots,
                                                               std::string *err) {
  if (err) err->clear();
  if (!mission.HaveWeather) return nullptr;
  switch (mission.Weather.Kind) {
    case FBWeatherKind::Calm:
      return std::make_unique<FBCalmWeather>();
    case FBWeatherKind::Wind:
      return std::make_unique<FBConstantWindWeather>(mission.Weather.WindFromDeg,
                                                     mission.Weather.WindSpeedKt);
    case FBWeatherKind::Fixture: {
      /* A bare name is an ASSET (it travels with the build); anything with a separator is a path the
       * mission author chose, used verbatim. */
      const std::string &name = mission.Weather.Fixture;
      std::string path = name;
      if (name.find('/') == std::string::npos) {
        if (roots.Assets.empty()) {
          if (err) *err = "this client has no asset root to resolve 'wx fixture " + name + "'";
          return nullptr;
        }
        path = roots.Assets + "/" + name;
      }
      auto fixed = std::make_unique<FBFixedWeather>(path);
      if (!fixed->Ok()) {
        if (err) *err = "cannot read weather fixture " + path;
        return nullptr;
      }
      return fixed;
    }
  }
  return nullptr;
}

} // namespace FlightBox::Missions
#endif
