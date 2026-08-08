/* THE DECLARED RUN, EXECUTED. A run scene is a viewer with unusual wishes — brackets around one
 * standpoint, products beside the picture — and this is a layer OVER Outshine, never a part of it:
 * nothing here builds a world. Both translations link it, because a recording that only one client
 * can make is a measurement about one client. */
#ifndef SCENERUNNER_H
#define SCENERUNNER_H

#include <cstdint>
#include <string>
#include <vector>

#include "Artifacts.h"
#include "Outshine.h"
#include "Scene.h"

namespace outshine::Clients {

class SceneRunner {
public:
  SceneRunner(Outshine &app, const Scene &scene, Artifacts &out)
      : App_(app), Scene_(scene), Out_(out) {}

  /* The subject bench replaces the world, so whether it runs is decided BEFORE Outshine::Open(). */
  bool IsSubjectBench() const;
  int RunSubject();
  int Run();

private:
  bool Warm();
  void Settle();
  void ReportSettled() const;
  void ReportCounters() const;

  int Dispatch(const Scene::Run &run);
  int Motion(const Scene::Run::MotionRun &m);
  int DumpClasses(const Scene::Run::ClassDumpRun &d) const;
  int CompareClasses() const;
  int ProbeWind(const Scene::Run::WindProbeRun &w) const;

  bool WritePng(const std::string &name);
  bool WriteDepth(const std::string &name) const;
  std::string FrameName(const std::string &path, int frame, const char *ext) const;

  Outshine &App_;
  const Scene &Scene_;
  Artifacts &Out_;
  int Warmed_ = 0, Settled_ = 0;
  std::vector<uint8_t> Rgba_;
};

} // namespace outshine::Clients
#endif
