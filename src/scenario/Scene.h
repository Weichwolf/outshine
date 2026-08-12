#ifndef SCENE_H
#define SCENE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Animation.h"
#include "ExposureParams.h"
#include "Fields.h"
#include "Json.h"
#include "Stage.h"

namespace outshine::Scenario {

/* ONE DECLARED SCENE, whole: what is being looked at, from where, when, and — if nobody is watching —
 * what is to be recorded there. The world half has NO defaults, because a default there would be
 * content nobody wrote; the recording half does, because a warm ceiling is a guard against a hung
 * upstream and not a picture.
 *
 * TWO AXES, AND WORLD-OR-NOT IS NOT ONE OF THEM. Camera and clock belong to the OBSERVER; the stage
 * is the SUBJECT (Stage.h), and choosing the studio arm removes the observer's place and civil time
 * rather than adding a dimension. A studio scene therefore has no latitude to declare — not "a
 * latitude that is ignored", which is what a boolean would have left behind.
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

  /* THE SIZE THE PICTURE IS PRODUCED AT, in pixels, declared by the scene and by nothing else — no
   * window, no canvas, no display. 1280x720 is the frame budget's subject [SET, CLAUDE.md];
   * a scene wanting another size has to say why, and the parser refuses one that does not. */
  static constexpr int kBudgetWidth = 1280, kBudgetHeight = 720;
  struct Resolution {
    int Width = kBudgetWidth, Height = kBudgetHeight;
    std::string Why;
  };

  /* ONE PRODUCT OF A RUN SCENE. Each kind reads its own parameter object rather than a shared flag
   * soup (C++ Core Guidelines I.23), and every either/or is an enumeration (Enum.2). */
  struct Run {
    enum class Kind { Motion, ClassDump, ClassCompare, WindProbe, Bench };
    /* Whether tiles keep arriving under the moving camera. A hitch on stream-in only exists while
     * the world is still answering, and a temporal filter is only judgeable while it is not. */
    enum class Stream { Frozen, Streaming };
    /* PICTURES OR THE SERIES, never both: PNG compression depends on the picture and would sit
     * inside every frame time, so a run that measures writes no image and a run that shows makes no
     * timing claim. */
    enum class Product { Stills, Profile };

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
    /* THE STUDIO MATRIX, RECORDED. What stands on the stage is the stage's statement, so nothing
     * here names a subject: this is where the pictures go and how many turntable steps they take. */
    struct BenchRun {
      std::string Dir = "bench";
      int TurnSteps = 8;
    };

    Kind What = Kind::Motion;
    MotionRun Motion;
    ClassDumpRun ClassDump;
    WindProbeRun WindProbe;
    BenchRun Bench;
  };

  /* `path` names this scene for a refusal — `<file>: scenes[3]` — so a message states where to look
   * without the caller having to prepend anything. */
  [[nodiscard]] bool Read(const Json::Ref &node, const std::string &path, std::string &err);

  const std::string &Id() const { return Id_; }
  [[nodiscard]] Kind What() const { return Kind_; }
  /* Why this scene exists, in the declaration's own words. Read into the run's identity line, so a
   * declared reason travels with the log the run produced and cannot rot unread. */
  const std::string &Why() const { return Why_; }

  const Scenario::Stage &Staged() const { return *Stage_; }

  double FovDeg() const { return FovDeg_; }

  /* THE SUB-PIXEL SAMPLE OFFSET, FROZEN, in pixels. Declared, because a pinned stochastic sequence
   * is part of what the scene is, and two scenes at two pinned phases are the only way to ask
   * whether the offset moved anything world-fixed. Absent leaves the Halton cycle running. */
  [[nodiscard]] bool HasJitterPin() const { return HasJitterPin_; }
  double JitterPinX() const { return JitterPin_[0]; }
  double JitterPinY() const { return JitterPin_[1]; }

  const ExposureParams &Exposure() const { return Exposure_; }
  const Resolution &RenderResolution() const { return Resolution_; }
  /* How much temporal history every delivered frame carries; < 0 asks the renderer for its own
   * settle length, which is the only honest default for a number the renderer owns. */
  int SettleFrames() const { return SettleFrames_; }
  const std::vector<Run> &Runs() const { return Runs_; }

private:
  [[nodiscard]] bool ReadStage(Fields &scene, std::string &err);
  [[nodiscard]] bool ReadWorld(const Json::Ref &node, const std::string &path, std::string &err);
  [[nodiscard]] bool ReadStudio(const Json::Ref &node, const std::string &path, std::string &err);
  [[nodiscard]] bool ReadSubject(const Json::Ref &node, const std::string &path, Subject &out,
                                 std::string &err);
  [[nodiscard]] bool ReadExposure(Fields &scene, std::string &err);
  [[nodiscard]] bool ReadResolution(Fields &scene, std::string &err);
  [[nodiscard]] bool ReadJitter(Fields &scene);
  [[nodiscard]] bool ReadRuns(Fields &scene, std::string &err);
  [[nodiscard]] bool ReadMotion(Fields &run, Run &out);

  std::string Id_, Why_;
  Kind Kind_ = Kind::Interactive;
  /* THE STAGE HAS NO DEFAULT ARM, so it is absent until the declaration says which one — a Scene
   * that failed to read has no stage to be misread as a world at the origin. */
  std::optional<Scenario::Stage> Stage_;
  double FovDeg_ = 0.0;
  double JitterPin_[2] = {0.0, 0.0};
  bool HasJitterPin_ = false;
  int SettleFrames_ = -1;
  ExposureParams Exposure_;
  Resolution Resolution_;
  std::vector<Run> Runs_;
};

} // namespace outshine::Scenario
#endif
