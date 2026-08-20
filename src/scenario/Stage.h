#ifndef STAGE_H
#define STAGE_H

#include <string>
#include <variant>

#include "Standpoint.h"
#include "Studio.h"

namespace outshine::Scenario {

struct WorldStage {

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

  std::string SnapshotPath;
};

class Stage {
public:
  explicit Stage(WorldStage world) : Arm_(std::move(world)) {}
  explicit Stage(StudioStage studio) : Arm_(std::move(studio)) {}

  const WorldStage *AsWorld() const noexcept { return std::get_if<WorldStage>(&Arm_); }
  const StudioStage *AsStudio() const noexcept { return std::get_if<StudioStage>(&Arm_); }

private:
  std::variant<WorldStage, StudioStage> Arm_;
};

}
#endif
