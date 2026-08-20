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

  const Xml::Ref kinds = root.Child("kinds");
  for (size_t at = 0; at < kinds.Count("kind"); ++at) {
    const Xml::Ref one = kinds.At("kind", at);
    Kind made;
    made.Name = one.Attr("name");
    made.Inherits = one.Attr("inherits");
    made.Asset = one.Attr("asset");
    made.Programme = one.Attr("programme");
    made.TickHz = one.Num("tickHz", 0.0);
    for (size_t which = 0; which < one.Count("may"); ++which) {
      made.Capabilities.push_back(one.At("may", which).Attr("do"));
    }
    for (size_t which = 0; which < one.Count("has"); ++which) {
      const Xml::Ref attribute = one.At("has", which);
      made.Attributes.push_back(Attribute{attribute.Attr("name"), attribute.Attr("value")});
    }
    into.Kinds.push_back(made);
  }

  const Xml::Ref instances = root.Child("instances");
  for (size_t at = 0; at < instances.Count("instance"); ++at) {
    const Xml::Ref one = instances.At("instance", at);
    Instance made;
    made.Of = one.Attr("of");
    made.Id = one.Attr("id");
    made.In = one.Attr("in");
    ReadVector(one, "x", "y", "z", made.TranslationM, 3);
    ReadVector(one, "qx", "qy", "qz", made.RotationXyzw, 3);
    made.RotationXyzw[3] = one.Num("qw", 1.0);
    for (size_t which = 0; which < one.Count("has"); ++which) {
      const Xml::Ref attribute = one.At("has", which);
      made.Attributes.push_back(Attribute{attribute.Attr("name"), attribute.Attr("value")});
    }
    for (size_t which = 0; which < one.Count("holds"); ++which) {
      made.Holds.push_back(one.At("holds", which).Attr("what"));
    }
    into.Instances.push_back(made);
  }

  const Xml::Ref regions = root.Child("regions");
  for (size_t at = 0; at < regions.Count("region"); ++at) {
    const Xml::Ref one = regions.At("region", at);
    Region made;
    made.Id = one.Attr("id");
    made.Kind = one.Attr("kind");
    ReadVector(one, "x", "y", "z", made.OriginM, 3);
    made.RadiusM = one.Num("radiusM", 0.0);
    made.Streams = one.Flag("streams", true);
    for (size_t which = 0; which < one.Count("uses"); ++which) {
      made.Uses.push_back(one.At("uses", which).Attr("what"));
    }
    into.Regions.push_back(made);
  }
  for (size_t at = 0; at < regions.Count("door"); ++at) {
    const Xml::Ref one = regions.At("door", at);
    Door made;
    made.Id = one.Attr("id");
    made.From = one.Attr("from");
    made.To = one.Attr("to");
    ReadVector(one, "x", "y", "z", made.AtM, 3);
    into.Doors.push_back(made);
  }

  const Xml::Ref volumes = root.Child("volumes");
  for (size_t at = 0; at < volumes.Count("volume"); ++at) {
    const Xml::Ref one = volumes.At("volume", at);
    Volume made;
    made.Id = one.Attr("id");
    made.In = one.Attr("in");
    made.Shape = one.Attr("shape", "box");
    ReadVector(one, "x", "y", "z", made.AtM, 3);
    made.ExtentM[0] = one.Num("extentX", 0.0);
    made.ExtentM[1] = one.Num("extentY", 0.0);
    made.ExtentM[2] = one.Num("extentZ", 0.0);
    made.Fires = one.Attr("fires");
    made.When = one.Attr("when", "enter");
    into.Volumes.push_back(made);
  }

  const Xml::Ref audio = root.Child("audio");
  for (size_t at = 0; at < audio.Count("bus"); ++at) {
    const Xml::Ref one = audio.At("bus", at);
    into.Buses.push_back(Bus{one.Attr("id"), one.Attr("into"), one.Num("gainDb", 0.0)});
  }
  for (size_t at = 0; at < audio.Count("sound"); ++at) {
    const Xml::Ref one = audio.At("sound", at);
    Sound made;
    made.Id = one.Attr("id");
    made.Uri = one.Attr("uri");
    made.Bus = one.Attr("bus");
    made.Positional = one.Flag("positional", false);
    made.Loops = one.Flag("loops", false);
    made.GainDb = one.Num("gainDb", 0.0);
    made.FalloffM = one.Num("falloffM", 0.0);
    into.Sounds.push_back(made);
  }

  const Xml::Ref tables = root.Child("tables");
  for (size_t at = 0; at < tables.Count("table"); ++at) {
    const Xml::Ref one = tables.At("table", at);
    Table made;
    made.Id = one.Attr("id");
    for (size_t which = 0; which < one.Count("column"); ++which) {
      made.Columns.push_back(one.At("column", which).Attr("name"));
    }
    for (size_t which = 0; which < one.Count("row"); ++which) {
      const Xml::Ref row = one.At("row", which);
      std::vector<std::string> cells;
      for (size_t cell = 0; cell < row.Count("cell"); ++cell) {
        cells.push_back(row.At("cell", cell).Attr("value"));
      }
      made.Rows.push_back(cells);
    }
    into.Tables.push_back(made);
  }

  const Xml::Ref events = root.Child("events");
  for (size_t at = 0; at < events.Count("event"); ++at) {
    const Xml::Ref one = events.At("event", at);
    Event made;
    made.Name = one.Attr("name");
    for (size_t which = 0; which < one.Count("carries"); ++which) {
      made.Carries.push_back(one.At("carries", which).Attr("what"));
    }
    into.Events.push_back(made);
  }

  const Xml::Ref views = root.Child("views");
  for (size_t at = 0; at < views.Count("view"); ++at) {
    const Xml::Ref one = views.At("view", at);
    View made;
    made.Id = one.Attr("id");
    made.Follows = one.Attr("follows");
    made.OffsetM[0] = one.Num("offsetX", 0.0);
    made.OffsetM[1] = one.Num("offsetY", 0.0);
    made.OffsetM[2] = one.Num("offsetZ", 0.0);
    made.FovDeg = one.Num("fovDeg", 0.0);
    made.TimeScale = one.Num("timeScale", 1.0);
    into.Views.push_back(made);
  }

  const Xml::Ref state = root.Child("state");
  for (size_t at = 0; at < state.Count("persist"); ++at) {
    into.State.push_back(Persisted{state.At("persist", at).Attr("what")});
  }

  return true;
}

} // namespace outshine
