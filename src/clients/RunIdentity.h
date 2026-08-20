#ifndef RUNIDENTITY_H
#define RUNIDENTITY_H

#include <string>

#include "Sanitisers.h"
#include "Telemetry.h"

namespace outshine::Clients {

class RunIdentity : public TelemetrySource {
public:

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
    schema.Add("san");
    schema.Add("build");
    schema.Add("agent");
  }
  void SampleTelemetry(TelemetryRow &row) const override {
    row.Push(F_.Mod);
    row.Push(F_.Scene);
    row.Push(F_.RenderW);
    row.Push(F_.RenderH);
    row.Push(F_.Client);
    row.Push(std::string(kSanitisers));
    row.Push(F_.Build);
    row.Push(F_.Agent);
  }

private:
  Fields F_;
};

}
#endif
