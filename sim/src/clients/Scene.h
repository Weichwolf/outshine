#ifndef SCENE_H
#define SCENE_H

#include <cstdint>
#include <string>
#include <vector>

#include "Animation.h"
#include "ExposureParams.h"
#include "Json.h"

namespace outshine::Clients {

/* ONE DECLARED SCENE, whole: where the eye stands, where it looks, when, what the air is doing —
 * and, if nobody is watching, what is to be recorded there. The world half has NO defaults, because
 * a default there would be content nobody wrote (doc/goal.md §1); the recording half does, because a
 * warm ceiling is a guard against a hung server and not a picture.
 *
 * The two kinds are DECLARED and not switched: an interactive scene is stood in, a run scene is
 * executed and delivers its products.
 *
 * A RECORDING IS A MOVING ONE. `Run` carries a frame count, a clock step and a path per frame
 * because the defects that cost the most — popping at a LOD change, ghosting and smear in the
 * temporal filter, a hitch on a stream-in, a shading that jumps at a mesh swap — are invisible in a
 * still. A still is the one-frame case of a motion, not the other way round. */
class Scene {
public:
  enum class Kind { Interactive, Run };

  /* The frame, and how much temporal history every delivered frame carries. `SettleFrames` < 0 asks
   * the renderer for its own settle length, which is the only honest default for a number the
   * renderer owns. There is no warm-up to bracket: the load runs until the world is there. */
  struct Capture {
    int Width = 1280, Height = 720;
    int SettleFrames = -1;
  };

  /* ONE PRODUCT OF A RUN SCENE. Each kind reads its own parameter object rather than a shared flag
   * soup (C++ Core Guidelines I.23), and every either/or is an enumeration (ES.9). */
  struct Run {
    enum class Kind { Motion, ClassDump, ClassCompare, WindProbe, Subject };
    /* Whether tiles keep arriving under the moving camera. A hitch on stream-in only exists while
     * the world is still answering, and a temporal filter is only judgeable while it is not. */
    enum class Stream { Frozen, Streaming };
    /* PICTURES OR THE SERIES, never both: PNG compression depends on the picture and would sit
     * inside every frame time, so a run that measures writes no image and a run that shows makes no
     * timing claim. */
    enum class Product { Stills, Profile };
    enum class Foliage { Bare, Leaves };

    /* `Path` with Frames == 1 is the file; with Frames > 1 it is a directory taking %04d.png
     * (Stills) or the CSV file (Profile). Every scene property the run moves is an animation
     * channel (Animation.h) — there is no second mechanism, and a still is the run with one frame
     * and no channels. The standpoint and the clocks are RESTORED at the end, so the scene's
     * declaration and not the previous run states where the next one starts.
     *
     * `Fps` only enters DERIVED seconds and the frame-budget verdict; 60 is CLAUDE.md's 720p60
     * budget [SET]. It never enters the frame count, which is what keeps a run reproducible. */
    struct MotionRun {
      int Frames = 1;
      double Fps = 60.0;
      Stream World = Stream::Frozen;
      Product Give = Product::Stills;
      std::string Path, Depth;
      Animation Move;
    };
    struct ClassDumpRun {
      std::string Path;
      double SpanM = 400.0, StepM = 0.05;
    };
    struct WindProbeRun {
      std::string Path;
      int Samples = 512, Frames = 240;
      double DxM = 0.03, DtS = 1.0 / 30.0;
    };
    struct SubjectRun {
      std::string Template, Species, Dir = "bench";
      double HeightM = 0.0;
      int TurnSteps = 8, LeafMult = 1;
      Foliage Leaf = Foliage::Leaves;
    };

    Kind What = Kind::Motion;
    MotionRun Motion;
    ClassDumpRun ClassDump;
    WindProbeRun WindProbe;
    SubjectRun Subject;
  };

  bool Read(const Render::Json::Ref &node, std::string &err);

  const std::string &Id() const { return Id_; }
  Kind What() const { return Kind_; }

  double Lat() const { return Lat_; }
  double Lon() const { return Lon_; }
  double EyeM() const { return EyeM_; }
  /* The LENS ALTITUDE a camera operator publishes. Absent leaves the eye at EyeM above the DEM. */
  bool HasLensAslM() const { return HasLensAslM_; }
  double LensAslM() const { return LensAslM_; }
  double YawDeg() const { return YawDeg_; }
  double PitchDeg() const { return PitchDeg_; }
  double FovDeg() const { return FovDeg_; }

  const std::string &Utc() const { return Utc_; }
  int64_t UtcS() const { return UtcS_; }

  double WindDeg() const { return WindDeg_; }
  double WindMs() const { return WindMs_; }
  double CloudCover() const { return CloudCover_; }
  double WindClockS() const { return WindClockS_; }

  double ViewM() const { return ViewKm_ * 1000.0; }
  double OrthoM() const { return OrthoM_; }
  const std::string &Snapshot() const { return Snapshot_; }

  const Render::ExposureParams &Exposure() const { return Exposure_; }
  const Capture &Recording() const { return Capture_; }
  const std::vector<Run> &Runs() const { return Runs_; }

private:
  bool ReadExposure(const Render::Json::Ref &node, std::string &err);
  bool ReadCapture(const Render::Json::Ref &node, std::string &err);
  bool ReadRuns(const Render::Json::Ref &node, std::string &err);
  bool ReadMotion(const Render::Json::Ref &node, Run &run, std::string &err);

  std::string Id_, Utc_, Snapshot_;
  Kind Kind_ = Kind::Interactive;
  double Lat_ = 0.0, Lon_ = 0.0, EyeM_ = 0.0, LensAslM_ = 0.0;
  double YawDeg_ = 0.0, PitchDeg_ = 0.0, FovDeg_ = 0.0;
  int64_t UtcS_ = 0;
  double WindDeg_ = 0.0, WindMs_ = 0.0, CloudCover_ = 0.0, WindClockS_ = 0.0;
  double ViewKm_ = 60.0, OrthoM_ = 0.0;
  bool HasLensAslM_ = false;
  Render::ExposureParams Exposure_;
  Capture Capture_;
  std::vector<Run> Runs_;
};

} // namespace outshine::Clients
#endif
