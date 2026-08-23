#include "ScenarioRead.h"

#include <outshine/Scenario.h>

#include <cstring>

#include "Xml.h"

namespace outshine {

const Asset *Scenario::Subject() const {
  for (const Asset &asset : Assets) {
    if (asset.Kind == "gltf") { return &asset; }
  }
  return nullptr;
}

namespace {

[[nodiscard]] bool Declares(const Xml::Ref &parent, const char *child) {
  const Xml::Ref::Siblings named = parent.Children(child);
  return named.begin() != named.end();
}

struct Element {
  const char *Path;
  const char *Children;
  const char *Attributes;
  const char *Required = "";
};

const Element kGrammar[] = {
    {"scenario",
     "world render lighting providers generators compositors assets placements surfaces kinds "
     "instances regions volumes audio tables events views vehicle player drive physics clock input state layer",
     "name version epoch decay active"},
    {"scenario/layer", "", "id path set", "path"},
    {"scenario/world", "", "lat lon radiusM gravityMs2 airDensityKgM3 windDeg windMs cloudCover"},
    {"scenario/render", "output stage",
     "widthPx heightPx fps fill orbitDegPerFrame transfer exposure precision"},
    {"scenario/render/output", "", "name", "name"},
    {"scenario/render/stage", "", "name", "name"},
    {"scenario/lighting", "key environment", ""},
    {"scenario/lighting/key", "", "lux elevationDeg bearingDeg"},
    {"scenario/lighting/environment", "", "r g b"},
    {"scenario/providers", "provider", ""},
    {"scenario/providers/provider", "", "kind pin rank whenAbsent", "kind"},
    {"scenario/generators", "generator", ""},
    {"scenario/generators/generator", "set", "kind", "kind"},
    {"scenario/generators/generator/set", "", "name value"},
    {"scenario/compositors", "compositor", ""},
    {"scenario/compositors/compositor", "", "kind budgetPx on", "kind"},
    {"scenario/assets", "asset", ""},
    {"scenario/assets/asset", "", "uri digest kind variant animation", "uri"},
    {"scenario/placements", "place", ""},
    {"scenario/placements/place", "", "asset x y z qx qy qz qw scale", "asset"},
    {"scenario/surfaces", "surface", ""},
    {"scenario/surfaces/surface", "",
     "document style programme leftFrac topFrac widthFrac heightFrac z", "document"},
    {"scenario/kinds", "kind", ""},
    {"scenario/kinds/kind", "may has mind", "name inherits asset", "name"},
    {"scenario/kinds/kind/mind", "", "tier uses programme prompt model meanwhile hz everyS stepBudget tokenBudget latencyBudgetMs temperature seed"},
    {"scenario/kinds/kind/may", "", "do", "do"},
    {"scenario/kinds/kind/has", "", "name value", "name"},
    {"scenario/instances", "instance", ""},
    {"scenario/instances/instance", "has holds", "of id in x y z qx qy qz qw", "of"},
    {"scenario/instances/instance/has", "", "name value", "name"},
    {"scenario/instances/instance/holds", "", "what", "what"},
    {"scenario/regions", "region door", ""},
    {"scenario/regions/region", "uses", "id kind x y z radiusM streams"},
    {"scenario/regions/region/uses", "", "what", "what"},
    {"scenario/regions/door", "", "id from to x y z", "from to"},
    {"scenario/volumes", "volume", ""},
    {"scenario/volumes/volume", "", "id in shape x y z extentX extentY extentZ fires when dwellS"},
    {"scenario/audio", "bus sound", ""},
    {"scenario/audio/bus", "", "id into gainDb"},
    {"scenario/audio/sound", "", "id uri bus positional loops gainDb falloffM", "uri"},
    {"scenario/tables", "table", ""},
    {"scenario/tables/table", "column row", "id"},
    {"scenario/tables/table/column", "", "name type", "name"},
    {"scenario/tables/table/row", "cell", ""},
    {"scenario/tables/table/row/cell", "", "value"},
    {"scenario/events", "event", ""},
    {"scenario/events/event", "carries", "name", "name"},
    {"scenario/events/event/carries", "", "what", "what"},
    {"scenario/views", "view", ""},
    {"scenario/views/view", "",
     "id follows person offsetX offsetY offsetZ distanceM pitchLimitDeg fovDeg timeScale"},
    {"scenario/player", "", "is starts view eyeHeightM walkMs runMs"},
    {"scenario/vehicle", "centreOfMass inertia contact tyre drive brake body seat",
     "name asset massKg widthM wheelbaseM assetWheelbase assetGround assetCentreX assetCentreZ turningCircleM trackM"},
    {"scenario/vehicle/centreOfMass", "", "x y z"},
    {"scenario/vehicle/inertia", "", "ixx iyy izz"},
    {"scenario/vehicle/contact", "",
     "at node x y z reachM stiffnessNPerM dampingNsPerM travelM stopNPerM limitN"},
    {"scenario/vehicle/tyre", "", "grip radiusM corneringNPerRad relaxationM"},
    {"scenario/vehicle/drive", "", "peakTorqueNm finalDrive"},
    {"scenario/vehicle/brake", "", "peakTorqueNm"},
    {"scenario/vehicle/body", "", "dragCoefficient frontalM2"},
    {"scenario/vehicle/seat", "", "at node x y z"},
    {"scenario/drive", "", "fromLat fromLon toLat toLon zoom"},
    {"scenario/physics", "", "dial"},
    {"scenario/clock", "", "start rate"},
    {"scenario/input", "bind", ""},
    {"scenario/input/bind", "", "event action", "event action"},
    {"scenario/state", "persist", ""},
    {"scenario/state/persist", "", "what", "what"},
};

bool Names(const char *list, const std::string &wanted) {
  const std::string all(list);
  size_t at = 0;
  while (at < all.size()) {
    const size_t stop = all.find(' ', at);
    const std::string one = all.substr(at, stop == std::string::npos ? std::string::npos : stop - at);
    if (one == wanted) { return true; }
    if (stop == std::string::npos) { break; }
    at = stop + 1;
  }
  return false;
}

const Element *Known(const std::string &path) {
  for (const Element &one : kGrammar) {
    if (path == one.Path) { return &one; }
  }
  return nullptr;
}

bool Grammatical(const Xml::Ref &node, const std::string &path, std::string &error) {
  const Element *const known = Known(path);
  if (known == nullptr) {
    error = "<" + node.Name() + "> is not a child this scenario's grammar declares at " + path;
    return false;
  }
  for (size_t at = 0; at < node.AttributeCount(); ++at) {
    const std::string named = node.AttributeAt(at);
    if (!Names(known->Attributes, named)) {
      error = "<" + node.Name() + "> carries the attribute '" + named +
              "', and the ones it may carry are: " +
              (*known->Attributes == 0 ? std::string("none") : std::string(known->Attributes));
      return false;
    }
  }
  for (const char *at = known->Required; *at != 0;) {
    const char *end = at;
    while (*end != 0 && *end != ' ') { ++end; }
    const std::string wanted(at, end);
    if (node.Attr(wanted.c_str()).empty()) {
      error = "<" + node.Name() + "> declares no '" + wanted +
              "', and without it the element names nothing";
      return false;
    }
    at = *end == ' ' ? end + 1 : end;
  }
  for (Xml::Ref child = node.First(); child.Valid(); child = child.Next()) {
    const std::string name = child.Name();
    if (!Names(known->Children, name)) {
      error = "<" + node.Name() + "> carries a <" + name +
              ">, and the children it may carry are: " +
              (*known->Children == 0 ? std::string("none") : std::string(known->Children));
      return false;
    }
    if (!Grammatical(child, path + "/" + name, error)) { return false; }
  }
  return true;
}

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
  into.Ground.Lat = from.Num("lat", into.Ground.Lat);
  into.Ground.Lon = from.Num("lon", into.Ground.Lon);
  into.Ground.RadiusM = from.Num("radiusM", into.Ground.RadiusM);
  into.Ground.GravityMs2 = from.Num("gravityMs2", into.Ground.GravityMs2);
  into.Ground.AirDensityKgM3 = from.Num("airDensityKgM3", into.Ground.AirDensityKgM3);
  into.Ground.WindDeg = from.Num("windDeg", into.Ground.WindDeg);
  into.Ground.WindMs = from.Num("windMs", into.Ground.WindMs);
  into.Ground.CloudCover = from.Num("cloudCover", into.Ground.CloudCover);
}

void ReadRender(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Render.Declared = true;
  if (Declares(from, "output")) { into.Render.Outputs.clear(); }
  if (Declares(from, "stage")) { into.Render.Stages.clear(); }
  into.Render.Frame.WidthPx = (int)from.Int("widthPx", into.Render.Frame.WidthPx);
  into.Render.Frame.HeightPx = (int)from.Int("heightPx", into.Render.Frame.HeightPx);
  into.Render.Fps = from.Num("fps", into.Render.Fps);
  into.Render.Fill = from.Num("fill", into.Render.Fill);
  into.Render.OrbitDegPerFrame = from.Num("orbitDegPerFrame", into.Render.OrbitDegPerFrame);
  into.Render.Transfer = from.Attr("transfer", into.Render.Transfer.c_str());
  into.Render.Exposure = from.Num("exposure", into.Render.Exposure);
  into.Render.Precision = from.Attr("precision", into.Render.Precision.c_str());
  for (const Xml::Ref output : from.Children("output")) {
    into.Render.Outputs.push_back(output.Attr("name"));
  }
  for (const Xml::Ref stage : from.Children("stage")) {
    into.Render.Stages.push_back(stage.Attr("name"));
  }
}

void ReadLighting(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Lit.Declared = true;
  const Xml::Ref key = from.Child("key");
  if (key.Valid()) {
    into.Lit.Key.Lux = key.Num("lux", into.Lit.Key.Lux);
    into.Lit.Key.ElevationDeg = key.Num("elevationDeg", into.Lit.Key.ElevationDeg);
    into.Lit.Key.BearingDeg = key.Num("bearingDeg", into.Lit.Key.BearingDeg);
  }
  const Xml::Ref environment = from.Child("environment");
  if (environment.Valid()) {
    into.Lit.Environment[0] = environment.Num("r", into.Lit.Environment[0]);
    into.Lit.Environment[1] = environment.Num("g", into.Lit.Environment[1]);
    into.Lit.Environment[2] = environment.Num("b", into.Lit.Environment[2]);
  }
}

}

bool ReadScenario(const char *text, size_t length, Scenario &into, std::string &error) {
  into = Scenario();
  return ReadScenarioInto(text, length, into, error);
}

bool ReadScenarioInto(const char *text, size_t length, Scenario &into, std::string &error) {
  Xml document;
  if (!document.Parse(text, length)) {
    error = document.Error();
    return false;
  }
  return ReadScenarioInto(document, into, error);
}

bool ReadScenarioInto(const Xml &document, Scenario &into, std::string &error) {
  const Xml::Ref root = document.Root();
  if (root.Name() != "scenario") {
    error = "a scenario's root element is <scenario> and this one is <" + root.Name() + ">";
    return false;
  }

  if (!Grammatical(root, "scenario", error)) { return false; }

  into.Named.Name = root.Attr("name");
  into.Named.Version = root.Attr("version");
  into.Named.Active = root.Attr("active");
  into.Named.Epoch = root.Num("epoch", 0.0);
  into.Named.Decay = root.Num("decay", 0.0);

  for (const Xml::Ref one : root.Children("layer")) {
    into.Layers.push_back(Layer{one.Attr("id"), one.Attr("path"), one.Attr("set")});
  }

  ReadWorld(root.Child("world"), into);
  ReadRender(root.Child("render"), into);
  ReadLighting(root.Child("lighting"), into);

  const Xml::Ref providers = root.Child("providers");
  for (const Xml::Ref one : providers.Children("provider")) {
    Provider made;
    made.Kind = one.Attr("kind");
    made.Pin = one.Attr("pin");
    made.Rank = (int)one.Int("rank", 0);
    made.WhenAbsent = one.Attr("whenAbsent");
    into.Providers.push_back(made);
  }

  const Xml::Ref generators = root.Child("generators");
  for (const Xml::Ref one : generators.Children("generator")) {
    Generator made;
    made.Kind = one.Attr("kind");
    for (const Xml::Ref parameter : one.Children("set")) {
      made.Parameters.push_back(Setting{parameter.Attr("name"), parameter.Attr("value")});
    }
    into.Generators.push_back(made);
  }

  const Xml::Ref compositors = root.Child("compositors");
  for (const Xml::Ref one : compositors.Children("compositor")) {
    Compositor made;
    made.Kind = one.Attr("kind");
    made.BudgetPx = one.Num("budgetPx", 0.0);
    made.On = one.Flag("on", true);
    into.Compositors.push_back(made);
  }

  const Xml::Ref assets = root.Child("assets");
  for (const Xml::Ref one : assets.Children("asset")) {
    Asset made;
    made.Uri = one.Attr("uri");
    made.Digest = one.Attr("digest");
    made.Kind = one.Attr("kind");
    made.Variant = one.Attr("variant");
    const std::string animation = one.Attr("animation", "play");
    if (animation == "play") {
      made.Animation = AssetAnimation::Play;
    } else if (animation == "ignore") {
      made.Animation = AssetAnimation::Ignore;
    } else if (animation == "driven") {
      made.Animation = AssetAnimation::Driven;
    } else {
      error = "<asset> declares animation='" + animation +
              "', and the three answers are play, ignore and driven";
      return false;
    }
    into.Assets.push_back(made);
  }

  const Xml::Ref placements = root.Child("placements");
  for (const Xml::Ref one : placements.Children("place")) {
    Placement made;
    made.Asset = one.Attr("asset");
    ReadVector(one, "x", "y", "z", made.TranslationM, 3);
    ReadVector(one, "qx", "qy", "qz", made.RotationXyzw, 3);
    made.RotationXyzw[3] = one.Num("qw", 1.0);
    made.Scale[0] = made.Scale[1] = made.Scale[2] = one.Num("scale", 1.0);
    into.Placements.push_back(made);
  }

  const Xml::Ref surfaces = root.Child("surfaces");
  for (const Xml::Ref one : surfaces.Children("surface")) {
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
  if (physics.Valid()) {
    into.Motion.Declared = true;
    into.Motion.Dial = physics.Attr("dial", into.Motion.Dial.c_str());
  }

  const Xml::Ref clock = root.Child("clock");
  if (clock.Valid()) {
    into.Time.Declared = true;
    into.Time.Start = clock.Attr("start", into.Time.Start.c_str());
    into.Time.Rate = clock.Num("rate", into.Time.Rate);
  }

  const Xml::Ref input = root.Child("input");
  for (const Xml::Ref one : input.Children("bind")) {
    into.Input.push_back(Binding{one.Attr("event"), one.Attr("action")});
  }

  const Xml::Ref kinds = root.Child("kinds");
  for (const Xml::Ref one : kinds.Children("kind")) {
    Kind made;
    made.Name = one.Attr("name");
    made.Inherits = one.Attr("inherits");
    made.Asset = one.Attr("asset");
    for (const Xml::Ref mind : one.Children("mind")) {
      Mind thinks;
      thinks.Tier = mind.Attr("tier");
      thinks.Uses = mind.Attr("uses");
      thinks.Programme = mind.Attr("programme");
      thinks.Prompt = mind.Attr("prompt");
      thinks.Model = mind.Attr("model");
      thinks.Meanwhile = mind.Attr("meanwhile");
      thinks.Hz = mind.Num("hz", 0.0);
      thinks.EverySeconds = mind.Num("everyS", 0.0);
      thinks.StepBudget = mind.Int("stepBudget", 0);
      thinks.TokenBudget = (int)mind.Int("tokenBudget", 0);
      thinks.LatencyBudgetMs = mind.Num("latencyBudgetMs", 0.0);
      thinks.Temperature = mind.Num("temperature", 0.0);
      thinks.Seed = mind.Int("seed", 0);
      made.Minds.push_back(thinks);
    }
    for (const Xml::Ref may : one.Children("may")) {
      made.Capabilities.push_back(may.Attr("do"));
    }
    for (const Xml::Ref attribute : one.Children("has")) {
      made.Attributes.push_back(Attribute{attribute.Attr("name"), attribute.Attr("value")});
    }
    into.Kinds.push_back(made);
  }

  const Xml::Ref instances = root.Child("instances");
  for (const Xml::Ref one : instances.Children("instance")) {
    Instance made;
    made.Of = one.Attr("of");
    made.Id = one.Attr("id");
    made.In = one.Attr("in");
    ReadVector(one, "x", "y", "z", made.TranslationM, 3);
    ReadVector(one, "qx", "qy", "qz", made.RotationXyzw, 3);
    made.RotationXyzw[3] = one.Num("qw", 1.0);
    for (const Xml::Ref attribute : one.Children("has")) {
      made.Attributes.push_back(Attribute{attribute.Attr("name"), attribute.Attr("value")});
    }
    for (const Xml::Ref holds : one.Children("holds")) {
      made.Holds.push_back(holds.Attr("what"));
    }
    into.Instances.push_back(made);
  }

  const Xml::Ref regions = root.Child("regions");
  for (const Xml::Ref one : regions.Children("region")) {
    Region made;
    made.Id = one.Attr("id");
    made.Kind = one.Attr("kind");
    ReadVector(one, "x", "y", "z", made.OriginM, 3);
    made.RadiusM = one.Num("radiusM", 0.0);
    made.Streams = one.Flag("streams", true);
    for (const Xml::Ref uses : one.Children("uses")) {
      made.Uses.push_back(uses.Attr("what"));
    }
    into.Regions.push_back(made);
  }
  for (const Xml::Ref one : regions.Children("door")) {
    Door made;
    made.Id = one.Attr("id");
    made.From = one.Attr("from");
    made.To = one.Attr("to");
    ReadVector(one, "x", "y", "z", made.AtM, 3);
    into.Doors.push_back(made);
  }

  const Xml::Ref volumes = root.Child("volumes");
  for (const Xml::Ref one : volumes.Children("volume")) {
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
    made.DwellS = one.Num("dwellS", 0.0);
    into.Volumes.push_back(made);
  }

  const Xml::Ref audio = root.Child("audio");
  for (const Xml::Ref one : audio.Children("bus")) {
    into.Buses.push_back(Bus{one.Attr("id"), one.Attr("into"), one.Num("gainDb", 0.0)});
  }
  for (const Xml::Ref one : audio.Children("sound")) {
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
  for (const Xml::Ref one : tables.Children("table")) {
    Table made;
    made.Id = one.Attr("id");
    for (const Xml::Ref column : one.Children("column")) {
      made.Columns.push_back(column.Attr("name"));
      const std::string kind = column.Attr("type", "text");
      if (kind != "text" && kind != "number") {
        error = "<column> declares type='" + kind + "', and a cell is text or number";
        return false;
      }
      made.Types.push_back(kind == "number");
    }
    for (const Xml::Ref row : one.Children("row")) {
      std::vector<std::string> cells;
      for (const Xml::Ref cell : row.Children("cell")) {
        cells.push_back(cell.Attr("value"));
      }
      made.Rows.push_back(cells);
    }
    into.Tables.push_back(made);
  }

  const Xml::Ref events = root.Child("events");
  for (const Xml::Ref one : events.Children("event")) {
    Event made;
    made.Name = one.Attr("name");
    for (const Xml::Ref carries : one.Children("carries")) {
      made.Carries.push_back(carries.Attr("what"));
    }
    into.Events.push_back(made);
  }

  const Xml::Ref views = root.Child("views");
  for (const Xml::Ref one : views.Children("view")) {
    View made;
    made.Id = one.Attr("id");
    made.Follows = one.Attr("follows");
    made.Person = one.Attr("person");
    made.DistanceM = one.Num("distanceM", 0.0);
    made.PitchLimitDeg = one.Num("pitchLimitDeg", 89.0);
    made.OffsetM[0] = one.Num("offsetX", 0.0);
    made.OffsetM[1] = one.Num("offsetY", 0.0);
    made.OffsetM[2] = one.Num("offsetZ", 0.0);
    made.FovDeg = one.Num("fovDeg", 0.0);
    made.TimeScale = one.Num("timeScale", 1.0);
    into.Views.push_back(made);
  }

  for (const Xml::Ref one : root.Children("vehicle")) {
    Vehicle made;
    made.Name = one.Attr("name");
    made.Asset = one.Attr("asset");
    made.MassKg = one.Num("massKg", 0.0);
    made.WidthM = one.Num("widthM", 0.0);
    made.WheelbaseM = one.Num("wheelbaseM", 0.0);
    made.AssetWheelbase = one.Num("assetWheelbase", 0.0);
    made.AssetGround = one.Num("assetGround", 0.0);
    made.AssetCentreX = one.Num("assetCentreX", 0.0);
    made.AssetCentreZ = one.Num("assetCentreZ", 0.0);
    made.TurningCircleM = one.Num("turningCircleM", 0.0);
    made.TrackM = one.Num("trackM", 0.0);
    const Xml::Ref centre = one.Child("centreOfMass");
    made.CentreOfMassM[0] = centre.Num("x", 0.0);
    made.CentreOfMassM[1] = centre.Num("y", 0.0);
    made.CentreOfMassM[2] = centre.Num("z", 0.0);

    const Xml::Ref inertia = one.Child("inertia");
    made.InertiaKgM2[0] = inertia.Num("ixx", 0.0);
    made.InertiaKgM2[1] = inertia.Num("iyy", 0.0);
    made.InertiaKgM2[2] = inertia.Num("izz", 0.0);
    for (const Xml::Ref touch : one.Children("contact")) {
      Contact wheel;
      wheel.At = touch.Attr("at");
      ReadVector(touch, "x", "y", "z", wheel.AtM, 3);
      wheel.ReachM = touch.Num("reachM", 0.0);
      wheel.StiffnessNPerM = touch.Num("stiffnessNPerM", 0.0);
      wheel.DampingNsPerM = touch.Num("dampingNsPerM", 0.0);
      wheel.TravelM = touch.Num("travelM", 0.0);
      wheel.StopNPerM = touch.Num("stopNPerM", 0.0);
      wheel.LimitN = touch.Num("limitN", 0.0);
      made.Contacts.push_back(wheel);
    }
    const Xml::Ref tyre = one.Child("tyre");
    made.Grip = tyre.Num("grip", 0.0);
    made.TyreRadiusM = tyre.Num("radiusM", 0.0);
    made.CorneringNPerRad = tyre.Num("corneringNPerRad", 0.0);
    made.RelaxationM = tyre.Num("relaxationM", 0.0);
    made.PeakTorqueNm = one.Child("drive").Num("peakTorqueNm", 0.0);
    made.FinalDrive = one.Child("drive").Num("finalDrive", 0.0);
    made.BrakeTorqueNm = one.Child("brake").Num("peakTorqueNm", 0.0);
    const Xml::Ref body = one.Child("body");
    made.DragCoefficient = body.Num("dragCoefficient", 0.0);
    made.FrontalM2 = body.Num("frontalM2", 0.0);
    const Xml::Ref seat = one.Child("seat");
    made.SeatAt = seat.Attr("at");
    ReadVector(seat, "x", "y", "z", made.SeatM, 3);
    into.Vehicles.push_back(made);
  }

  const Xml::Ref player = root.Child("player");
  if (player.Valid()) {
    into.Played.Declared = true;
    into.Played.Is = player.Attr("is", into.Played.Is.c_str());
    into.Played.Starts = player.Attr("starts", into.Played.Starts.c_str());
    into.Played.View = player.Attr("view", into.Played.View.c_str());
    into.Played.EyeHeightM = player.Num("eyeHeightM", into.Played.EyeHeightM);
    into.Played.WalkMs = player.Num("walkMs", into.Played.WalkMs);
    into.Played.RunMs = player.Num("runMs", into.Played.RunMs);
  }

  const Xml::Ref drive = root.Child("drive");
  if (drive.Valid()) {
    into.Driven.Declared = true;
    into.Driven.FromLatDeg = drive.Num("fromLat", into.Driven.FromLatDeg);
    into.Driven.FromLonDeg = drive.Num("fromLon", into.Driven.FromLonDeg);
    into.Driven.ToLatDeg = drive.Num("toLat", into.Driven.ToLatDeg);
    into.Driven.ToLonDeg = drive.Num("toLon", into.Driven.ToLonDeg);
    into.Driven.Zoom = (int)drive.Num("zoom", (double)into.Driven.Zoom);
  }

  const Xml::Ref state = root.Child("state");
  for (const Xml::Ref persist : state.Children("persist")) {
    into.State.push_back(Persisted{persist.Attr("what")});
  }

  return true;
}

}
