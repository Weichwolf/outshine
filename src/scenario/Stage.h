/* WHAT THE OBSERVER IS LOOKING AT: a place on Earth, or a studio with no place at all. It is the
 * SUBJECT and not a third axis beside camera and clock — choosing the studio arm removes the world's
 * fields rather than adding a dimension, and it removes them by leaving no member to hold them.
 *
 * One discriminant, held by std::variant, so "a studio scene with a latitude" and "a world scene with
 * a declared key light" are both unspellable rather than refused. */
#ifndef STAGE_H
#define STAGE_H

#include <string>
#include <variant>

#include "Standpoint.h"
#include "Studio.h"

namespace outshine::Scenario {

/* THE PLACE, THE CLOCK AND THE AIR — everything that only exists because there is an Earth under the
 * scene. `UtcS` is seconds since the epoch, derived from the declared `utc` at load. */
struct WorldStage {
  /* THE PLACE COMES FIRST AND IS THE ONLY THING WITHOUT A DEFAULT (`C.41`): there is no world stage
   * whose standpoint has not been settled, so no arm of this type can exist at latitude nobody chose. */
  explicit WorldStage(Standpoint where) : Where(where) {}

  Standpoint Where;
  double EyeAglM = 0.0;
  double LensAslM = 0.0;
  bool HasLensAslM = false;
  double YawDeg = 0.0, PitchDeg = 0.0;
  std::string Utc;
  int64_t UtcS = 0;
  double WindFromDeg = 0.0, WindMs = 0.0, CloudCover = 0.0, WindClockS = 0.0;
  double ViewM = 60000.0, OrthoM = 0.0;
  /* A standpoint another client wrote and this one stands at instead (clients/Snapshot.h). */
  std::string SnapshotPath;
};

class Stage {
public:
  explicit Stage(WorldStage world) : Arm_(std::move(world)) {}
  explicit Stage(StudioStage studio) : Arm_(std::move(studio)) {}

  /* Null is the other arm, which is the only answer a caller can act on: there is no world stage
   * with an absent standpoint to hand back instead (`F.60`). */
  const WorldStage *AsWorld() const noexcept { return std::get_if<WorldStage>(&Arm_); }
  const StudioStage *AsStudio() const noexcept { return std::get_if<StudioStage>(&Arm_); }

private:
  std::variant<WorldStage, StudioStage> Arm_;
};

} // namespace outshine::Scenario
#endif
