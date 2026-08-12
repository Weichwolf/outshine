/* A STANDPOINT, written by the client that was steered and re-rendered by the one that measures.
 *
 * It is a bench artefact and NOT a second scene declaration: the scene is REFERENCED, and the
 * `scene` block is an identity that Matches() checks rather than content anyone may apply. Only
 * `camera` — the four numbers the walker actually owns — reaches the renderer. Without that split a
 * snapshot would silently become a place to declare a scene from, and nothing would notice the day
 * the two disagreed. */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Clients {

class Scene;

class Snapshot {
public:
  /* The basename the frame was filed under; the line names its own PNG so a line cut out of the log
   * still points at the picture it describes. */
  void SetName(const char *name);
  void SetScene(const Scene &s);
  void SetCamera(double lat, double lon, double yawDeg, double pitchDeg);
  /* Answers the reproducing client already has of its own, kept so the two can be SUBTRACTED: a DEM
   * or an ephemeris that disagreed is the one difference no pixel comparison would attribute. */
  void SetDerived(double groundM, double altAslM, double sunElDeg, double sunAzDeg);
  void SetClient(const char *name, double clockMs);

  std::string Text() const;

  [[nodiscard]] bool Load(const char *path);
  [[nodiscard]] bool LoadText(const char *text, size_t len);
  [[nodiscard]] bool Matches(const Scene &s);

  const std::string &Error() const { return Error_; }
  const std::string &Name() const { return Name_; }
  const std::string &SceneId() const { return ScenePath_; }
  const std::string &Client() const { return Client_; }

  double Lat() const { return Lat_; }
  double Lon() const { return Lon_; }
  double YawDeg() const { return YawDeg_; }
  double PitchDeg() const { return PitchDeg_; }
  double GroundM() const { return GroundM_; }
  double AltAslM() const { return AltAslM_; }
  double SunElDeg() const { return SunElDeg_; }
  double SunAzDeg() const { return SunAzDeg_; }

private:
  std::string Name_, ScenePath_, Client_, Error_;
  double SceneLat_ = 0.0, SceneLon_ = 0.0, SceneEyeM_ = 0.0;
  double SceneYawDeg_ = 0.0, ScenePitchDeg_ = 0.0, SceneFovDeg_ = 0.0;
  int64_t SceneUtcS_ = 0;
  double Lat_ = 0.0, Lon_ = 0.0, YawDeg_ = 0.0, PitchDeg_ = 0.0;
  double GroundM_ = 0.0, AltAslM_ = 0.0, SunElDeg_ = 0.0, SunAzDeg_ = 0.0;
  double ClockMs_ = 0.0;
};

} // namespace outshine::Clients
#endif
