#include <outshine/Scenario.h>

#include <cstring>

#include "Xml.h"

namespace outshine {

const Asset *Scenario::Subject(void) const {
  for (const Asset &asset : Assets) {
    if (asset.Kind == "gltf") { return &asset; }
  }
  return nullptr;
}

namespace {

void ReadVector(const Xml::Ref &from, const char *x, const char *y, const char *z, double *into,
                size_t count) {
  const char *names[4] = {x, y, z, "w"};
  for (size_t at = 0; at < count && at < 4; ++at) {
    into[at] = from.Num(names[at], into[at]);
  }
}

void ReadWorld(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Ground.Declared = true;
  into.Ground.Lat = from.Num("lat", 0.0);
  into.Ground.Lon = from.Num("lon", 0.0);
  into.Ground.RadiusM = from.Num("radiusM", 0.0);
  into.Ground.WindDeg = from.Num("windDeg", 0.0);
  into.Ground.WindMs = from.Num("windMs", 0.0);
  into.Ground.CloudCover = from.Num("cloudCover", 0.0);
}

void ReadRender(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Render.Frame.WidthPx = (int)from.Int("widthPx", into.Render.Frame.WidthPx);
  into.Render.Frame.HeightPx = (int)from.Int("heightPx", into.Render.Frame.HeightPx);
  into.Render.Fps = from.Num("fps", into.Render.Fps);
  into.Render.Fill = from.Num("fill", into.Render.Fill);
  into.Render.OrbitDegPerFrame = from.Num("orbitDegPerFrame", into.Render.OrbitDegPerFrame);
  into.Render.Transfer = from.Attr("transfer", into.Render.Transfer.c_str());
  into.Render.Exposure = from.Num("exposure", into.Render.Exposure);
  into.Render.Precision = from.Attr("precision", into.Render.Precision.c_str());
  for (size_t at = 0; at < from.Count("output"); ++at) {
    into.Render.Outputs.push_back(from.At("output", at).Attr("name"));
  }
  for (size_t at = 0; at < from.Count("stage"); ++at) {
    into.Render.Stages.push_back(from.At("stage", at).Attr("name"));
  }
}

void ReadLighting(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  const Xml::Ref key = from.Child("key");
  if (key.Valid()) {
    into.Lit.Key.Lux = key.Num("lux", 0.0);
    into.Lit.Key.ElevationDeg = key.Num("elevationDeg", 0.0);
    into.Lit.Key.BearingDeg = key.Num("bearingDeg", 0.0);
  }
  const Xml::Ref environment = from.Child("environment");
  if (environment.Valid()) {
    into.Lit.Environment[0] = environment.Num("r", 0.0);
    into.Lit.Environment[1] = environment.Num("g", 0.0);
    into.Lit.Environment[2] = environment.Num("b", 0.0);
  }
}

} // namespace

bool ReadScenario(const char *text, size_t length, Scenario &into, std::string &error) {
  Xml document;
  if (!document.Parse(text, length)) {
    error = document.Error();
    return false;
  }
  const Xml::Ref root = document.Root();
  if (root.Name() != "scenario") {
    error = "a scenario's root element is <scenario> and this one is <" + root.Name() + ">";
    return false;
  }

  into = Scenario();
  into.Named.Name = root.Attr("name");
  into.Named.Version = root.Attr("version");
  into.Named.Epoch = root.Num("epoch", 0.0);
  into.Named.Decay = root.Num("decay", 0.0);

  for (size_t at = 0; at < root.Count("layer"); ++at) {
    const Xml::Ref one = root.At("layer", at);
    into.Layers.push_back(Layer{one.Attr("id"), one.Attr("path")});
  }

  ReadWorld(root.Child("world"), into);
  ReadRender(root.Child("render"), into);
  ReadLighting(root.Child("lighting"), into);

  const Xml::Ref providers = root.Child("providers");
  for (size_t at = 0; at < providers.Count("provider"); ++at) {
    const Xml::Ref one = providers.At("provider", at);
    Provider made;
    made.Kind = one.Attr("kind");
    made.Pin = one.Attr("pin");
    made.Rank = (int)one.Int("rank", 0);
    made.WhenAbsent = one.Attr("whenAbsent");
    into.Providers.push_back(made);
  }

  const Xml::Ref generators = root.Child("generators");
  for (size_t at = 0; at < generators.Count("generator"); ++at) {
    const Xml::Ref one = generators.At("generator", at);
    Generator made;
    made.Kind = one.Attr("kind");
    for (size_t which = 0; which < one.Count("set"); ++which) {
      const Xml::Ref parameter = one.At("set", which);
      made.Parameters.push_back(Setting{parameter.Attr("name"), parameter.Attr("value")});
    }
    into.Generators.push_back(made);
  }

  const Xml::Ref compositors = root.Child("compositors");
  for (size_t at = 0; at < compositors.Count("compositor"); ++at) {
    const Xml::Ref one = compositors.At("compositor", at);
    Compositor made;
    made.Kind = one.Attr("kind");
    made.BudgetPx = one.Num("budgetPx", 0.0);
    made.On = one.Flag("on", true);
    into.Compositors.push_back(made);
  }

  const Xml::Ref assets = root.Child("assets");
  for (size_t at = 0; at < assets.Count("asset"); ++at) {
    const Xml::Ref one = assets.At("asset", at);
    Asset made;
    made.Uri = one.Attr("uri");
    made.Digest = one.Attr("digest");
    made.Kind = one.Attr("kind");
    made.Variant = one.Attr("variant");
    into.Assets.push_back(made);
  }

  const Xml::Ref placements = root.Child("placements");
  for (size_t at = 0; at < placements.Count("place"); ++at) {
    const Xml::Ref one = placements.At("place", at);
    Placement made;
    made.Asset = one.Attr("asset");
    ReadVector(one, "x", "y", "z", made.TranslationM, 3);
    ReadVector(one, "qx", "qy", "qz", made.RotationXyzw, 3);
    made.RotationXyzw[3] = one.Num("qw", 1.0);
    made.Scale[0] = made.Scale[1] = made.Scale[2] = one.Num("scale", 1.0);
    into.Placements.push_back(made);
  }

  const Xml::Ref surfaces = root.Child("surfaces");
  for (size_t at = 0; at < surfaces.Count("surface"); ++at) {
    const Xml::Ref one = surfaces.At("surface", at);
    Surface made;
    made.Document = one.Attr("document");
    made.Style = one.Attr("style");
    made.Programme = one.Attr("programme");
    made.LeftFrac = one.Num("leftFrac", 0.0);
    made.TopFrac = one.Num("topFrac", 0.0);
    made.WidthFrac = one.Num("widthFrac", 1.0);
    made.HeightFrac = one.Num("heightFrac", 1.0);
    made.Z = (int)one.Int("z", 0);
    into.Surfaces.push_back(made);
  }

  const Xml::Ref actors = root.Child("actors");
  for (size_t at = 0; at < actors.Count("actor"); ++at) {
    const Xml::Ref one = actors.At("actor", at);
    Actor made;
    made.Kind = one.Attr("kind");
    made.Programme = one.Attr("programme");
    made.Spawn = one.Attr("spawn");
    made.TickHz = one.Num("tickHz", 0.0);
    for (size_t which = 0; which < one.Count("may"); ++which) {
      made.Capabilities.push_back(one.At("may", which).Attr("do"));
    }
    into.Actors.push_back(made);
  }

  const Xml::Ref physics = root.Child("physics");
  if (physics.Valid()) { into.Motion.Dial = physics.Attr("dial"); }

  const Xml::Ref clock = root.Child("clock");
  if (clock.Valid()) {
    into.Time.Start = clock.Attr("start");
    into.Time.Rate = clock.Num("rate", 1.0);
  }

  const Xml::Ref input = root.Child("input");
  for (size_t at = 0; at < input.Count("bind"); ++at) {
    const Xml::Ref one = input.At("bind", at);
    into.Input.push_back(Binding{one.Attr("event"), one.Attr("action")});
  }

  const Xml::Ref state = root.Child("state");
  for (size_t at = 0; at < state.Count("persist"); ++at) {
    into.State.push_back(Persisted{state.At("persist", at).Attr("what")});
  }

  return true;
}

} // namespace outshine
