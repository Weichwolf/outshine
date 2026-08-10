/* WHAT EVERY TELEMETRY ROW HAS TO CARRY to be worth anything a week later: which mod, which scene,
 * which translation, which build, which browser. A run manifest beside the file would be one more
 * thing that can get separated from it — a row that cannot say what produced it is not a
 * measurement (CLAUDE.md). Registered FIRST on the bus, so the identity is the leading columns. */
#ifndef RUNIDENTITY_H
#define RUNIDENTITY_H

#include <string>

#include "Telemetry.h"

namespace outshine::Clients {

class RunIdentity : public TelemetrySource {
public:
  /* `Build` is the wasm hash in the browser and the binary's hash natively; `Agent` is the browser's
   * own version string and empty natively, where there is no browser to pin. `RenderW`/`RenderH` are
   * the scene's declared render size in pixels — the canvas the picture ends up on is not it. */
  struct Fields {
    std::string Mod, Scene, Client, Build, Agent;
    int RenderW = 0, RenderH = 0;
  };

  explicit RunIdentity(const Fields &f) : F_(f) {}

  const char *TelemetryName() const override { return "run"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override {
    schema.Add("mod");
    schema.Add("scene");
    schema.Add("renderW", "px");
    schema.Add("renderH", "px");
    schema.Add("client");
    schema.Add("build");
    schema.Add("agent");
  }
  void SampleTelemetry(TelemetryRow &row) const override {
    row.Push(F_.Mod);
    row.Push(F_.Scene);
    row.Push(F_.RenderW);
    row.Push(F_.RenderH);
    row.Push(F_.Client);
    row.Push(F_.Build);
    row.Push(F_.Agent);
  }

private:
  Fields F_;
};

} // namespace outshine::Clients
#endif
