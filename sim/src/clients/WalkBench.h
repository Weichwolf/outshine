/* THE FRAME ORACLE'S COMMAND LINE. A bench is a viewer with unusual wishes — brackets around the one
 * declared scene, extra products beside the picture — and it is a layer OVER Outshine, never a part
 * of it: nothing here may build a world, and everything here is refused to the browser on purpose. */
#ifndef WALKBENCH_H
#define WALKBENCH_H

#include <string>
#include <vector>

#include "Outshine.h"

namespace outshine::Clients {

class WalkBench {
public:
  bool Parse(int argc, char **argv);
  int Run();

private:
  int RunRig(Outshine &app);
  int RunScene(Outshine &app);
  bool Warm(Outshine &app);
  bool DumpClasses(Outshine &app) const;
  bool CompareClasses(Outshine &app) const;
  void Spin(Outshine &app) const;
  bool Sequence(Outshine &app);
  bool ProbeWind(Outshine &app) const;
  void ReportSettled(Outshine &app) const;
  void ReportCounters(Outshine &app) const;
  bool WriteImage(Outshine &app, const char *path) const;
  bool WritePng(Outshine &app, const char *path) const;
  bool WriteDepth(Outshine &app, const char *path) const;
  static void Usage(const char *argv0);

  int Width_ = 1280, Height_ = 720, Warm_ = 4000, WalkPasses_ = 240, Settle_ = -1;
  int Warmed_ = 0, SettleFrames_ = 0;
  double ViewKm_ = 60.0;
  std::string Base_ = "http://localhost:8081", Out_ = "walk.png", DepthPath_;
  int Bench_ = 0, Spin_ = 0;
  bool ManualEv_ = false;
  double EvStops_ = 0.0;
  double EyeOverrideM_ = -1.0, EyeAslM_ = -1.0e9;
  double PitchDeg_ = 1.0e9, YawDeg_ = 1.0e9;
  double StepE_ = 0.0, StepN_ = 0.0, WalkE_ = 0.0, WalkN_ = 0.0;
  std::string ClassDump_;
  double ClassSpan_ = 400.0, ClassStep_ = 0.05;
  bool ClassCmp_ = false;
  double WindT_ = 0.0, WindDeg_ = 1.0e9, WindMs_ = -1.0;
  int SeqFrames_ = 0;
  double SeqDt_ = 1.0 / 30.0;
  double SeqYawDeg_ = 0.0, SeqStepE_ = 0.0, SeqStepN_ = 0.0;
  std::string SeqOut_ = "seq", WindProbe_, SeqProf_;
  std::string RigTemplate_, RigSpecies_, RigOut_ = "bench";
  std::string ScenePath_;
  double OrthoM_ = 0.0;
  bool RigLeaves_ = true;
  int RigLeafMult_ = 1;
  std::string SnapshotPath_;
  double RigHeightM_ = 0.0;
  int RigTurn_ = 8;
  std::vector<uint8_t> Rgba_;
};

} // namespace outshine::Clients
#endif
