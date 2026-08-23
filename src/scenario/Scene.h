#ifndef OUTSHINE_SCENARIO_SCENE_H
#define OUTSHINE_SCENARIO_SCENE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Animation.h"
#include "ExposureParams.h"
#include "Fields.h"
#include "Json.h"
#include "Stage.h"

namespace outshine::SceneLegacy {

class Scene {
public:
  enum class Kind { Interactive, Run };

  static constexpr int kBudgetWidth = 1280, kBudgetHeight = 720;
  struct Resolution {
    int Width = kBudgetWidth, Height = kBudgetHeight;
    std::string Why;
  };

  struct Run {
    enum class Kind { Motion, ClassDump, ClassCompare, WindProbe, Bench };

    enum class Stream { Frozen, Streaming };

    enum class Product { Stills, Profile };

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

  [[nodiscard]] bool Read(const Json::Ref &node, const std::string &path, std::string &err);

  const std::string &Id() const { return Id_; }
  [[nodiscard]] Kind What() const { return Kind_; }

  const std::string &Why() const { return Why_; }

  const SceneLegacy::Stage &Staged() const { return *Stage_; }

  double FovDeg() const { return FovDeg_; }

  [[nodiscard]] bool HasJitterPin() const { return HasJitterPin_; }
  double JitterPinX() const { return JitterPin_[0]; }
  double JitterPinY() const { return JitterPin_[1]; }

  const ExposureParams &Exposure() const { return Exposure_; }
  const Resolution &RenderResolution() const { return Resolution_; }

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

  std::optional<SceneLegacy::Stage> Stage_;
  double FovDeg_ = 0.0;
  double JitterPin_[2] = {0.0, 0.0};
  bool HasJitterPin_ = false;
  int SettleFrames_ = -1;
  ExposureParams Exposure_;
  Resolution Resolution_;
  std::vector<Run> Runs_;
};

}
#endif
