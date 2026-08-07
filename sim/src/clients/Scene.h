#ifndef SCENE_H
#define SCENE_H

#include <cstdint>
#include <string>

#include "ExposureParams.h"
#include "Json.h"

namespace outshine::Clients {

/* A bench declaration, whole: where the eye stands, where it looks, when, and what the air is doing.
 * EVERY field is required and none has a default — the file is the complete statement of a scene, so
 * a default here would be content nobody wrote (doc/goal.md §1). Nothing visible is declared: the
 * engine produces it from this position. */
class Scene {
public:
  bool Load(const char *path);
  const std::string &Error() const { return Error_; }
  const std::string &Path() const { return Path_; }

  double Lat() const { return Lat_; }
  double Lon() const { return Lon_; }
  double EyeM() const { return EyeM_; }
  double YawDeg() const { return YawDeg_; }
  double PitchDeg() const { return PitchDeg_; }
  double FovDeg() const { return FovDeg_; }

  const std::string &Utc() const { return Utc_; }
  int64_t UtcS() const { return UtcS_; }

  double WindDeg() const { return WindDeg_; }
  double WindMs() const { return WindMs_; }
  double CloudCover() const { return CloudCover_; }

  /* The ONE optional block, and it is optional because "auto, no compensation" is not a choice a
   * scene has to state. Everything else in the file stays required. */
  const Render::ExposureParams &Exposure() const { return Exposure_; }

private:
  bool ParseExposure(const Render::Json::Ref &root);

  std::string Path_, Error_, Utc_;
  double Lat_ = 0.0, Lon_ = 0.0, EyeM_ = 0.0;
  double YawDeg_ = 0.0, PitchDeg_ = 0.0, FovDeg_ = 0.0;
  int64_t UtcS_ = 0;
  double WindDeg_ = 0.0, WindMs_ = 0.0, CloudCover_ = 0.0;
  Render::ExposureParams Exposure_;
};

} // namespace outshine::Clients
#endif
