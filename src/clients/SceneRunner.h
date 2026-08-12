/* THE DECLARED RUN, EXECUTED. A run scene is a viewer with unusual wishes — brackets around one
 * standpoint, products beside the picture — and this is a layer OVER Outshine, never a part of it:
 * nothing here builds a world. Both translations link it, because a recording that only one client
 * can make is a measurement about one client.
 *
 * IT IS A STATE MACHINE AND NOT A LOOP, and that is the whole shape of it: bring-up waits on a
 * device and on a tile server, a readback waits on the GPU, and in the browser every one of those
 * completes on the very thread a loop would be standing on. So a run advances by TURNS — the client
 * gives it one, it does at most one frame or one product's worth of work, and gives the turn back.
 * Both entry points then say the same two lines, and neither owns any part of the sequence. */
#ifndef SCENERUNNER_H
#define SCENERUNNER_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Artifacts.h"
#include "Outshine.h"
#include "Scene.h"
#include "SubjectBench.h"
#include "TreeFoliage.h"
#include "TreeMesh.h"

namespace outshine::Clients {

class SceneRunner {
public:
  using Scene = Scenario::Scene;

  SceneRunner(Outshine &app, const Scene &scene, Artifacts &out)
      : App_(app), Scene_(scene), Out_(out) {}

  enum class Progress { Running, Done };

  /* One turn. `Result()` is the run's exit code and stands once this answers Done. */
  [[nodiscard]] Progress Step();
  int Result() const { return Rc_; }

private:
  /* WHERE THE RUN STANDS. Every name is a thing being waited for, so a state nobody can leave says
   * in one word what it is waiting for. */
  enum class Stage { BringUp, Dispatch, Settle, SettleDrain, Frame, Still, Depth, FrameDrain,
                     MotionClose, CompareViz, CompareDepth, Irradiance, Exposure, Bench, Deliver,
                     Done };

  void BringUp();
  void Dispatch();
  void Finish(int rc);

  void MotionBegin(const Scene::Run::MotionRun &m);
  void MotionApply(double f);
  void MotionFrame();
  void MotionRow();
  void MotionNextFrame();
  void MotionClose();
  void WriteStill();
  void WriteDepth();

  void Settled();
  void Counters();
  void CompareClasses();

  int DumpClasses(const Scene::Run::ClassDumpRun &d) const;
  int ProbeWind(const Scene::Run::WindProbeRun &w) const;
  /* THE STUDIO, MADE READY. Each arm of the declared subject resolves its own two numbers — the
   * height the framings are derived from, and the name the files carry — and nothing else. */
  [[nodiscard]] bool BenchBegin();
  [[nodiscard]] bool GrowSubject(const Scenario::TreeSubject &declared, SubjectBench::Setup &setup);
  [[nodiscard]] bool StandSubject(const Scenario::SwardSubject &declared,
                                  SubjectBench::Setup &setup);

  std::string FrameName(const std::string &path, int frame, const char *ext) const;

  Outshine &App_;
  const Scene &Scene_;
  Artifacts &Out_;

  Stage Stage_ = Stage::BringUp;
  int Rc_ = 0;
  size_t RunIx_ = 0;
  double StageFromMs_ = 0.0;

  /* THE MOTION IN PROGRESS. Borrowed from the scene, which outlives the run. */
  const Scene::Run::MotionRun *Move_ = nullptr;
  Outshine::Stance Base_{};
  double LonPerM_ = 0.0;
  bool Moves_ = false, Profile_ = false;
  int Settle_ = 0, SettleAt_ = 0, FrameAt_ = 0;
  std::chrono::steady_clock::time_point T0_, T1_, T2_;
  std::string Csv_;
  std::vector<double> Ms_;

  std::vector<uint8_t> Rgba_;
  std::vector<float> Depth_;

  /* A STUDIO STAGE TAKES THE BINARY OVER COMPLETELY: no world, no tile stream, no wire, no
   * ephemeris. The grown mesh and its foliage are held for the whole bench run, because the arrays
   * are uploaded once and the numbers logged come off the same objects the picture was drawn from. */
  std::unique_ptr<SubjectBench> Bench_;
  Generators::TreeMesh BenchMesh_;
  Generators::TreeFoliage BenchFoliage_;
};

} // namespace outshine::Clients
#endif
