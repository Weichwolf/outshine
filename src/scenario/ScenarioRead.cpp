#include <cmath>
#include "ScenarioRead.h"

#include <Scenario.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include "Xml.h"

namespace outshine {

double Camera::exposureScale() const {
  if (!exposed()) { return 0.0; }
  const double ev100 =
      std::log2(ApertureFStops * ApertureFStops / ShutterS) - std::log2(SensitivityIso / 100.0);
  return 1.0 / (1.2 * std::pow(2.0, ev100));
}

const Asset *Scenario::subject() const {
  for (const Asset &asset : Assets) {
    if (asset.Kind == "gltf") { return &asset; }
  }
  return nullptr;
}

namespace {

[[nodiscard]] bool Declares(const Xml::Ref &parent, const char *child) {
  const Xml::Ref::Siblings named = parent.Children(child);
  return named.begin() != outshine::Xml::Ref::Siblings::end();
}

struct Element {
  const char *Path;
  const char *Children;
  const char *Required = "";
  const char *Allowed = "";
};

const Element kGrammar[] = {
    {.Path = "scenario",
     .Children =
         "world render lighting providers generators compositors assets placements surfaces kinds "
         "instances regions volumes audio tables events views body player drive physics clock "
         "scene "
         "input state layer"},
    {.Path = "scenario/layer", .Children = "", .Required = "path"},
    {.Path = "scenario/scene", .Children = ""},
    {.Path = "scenario/world", .Children = "relief osm"},
    {.Path = "scenario/world/relief", .Children = "", .Required = "kind"},
    {.Path = "scenario/world/osm", .Children = "way area"},
    {.Path = "scenario/world/osm/way", .Children = "", .Required = "kind"},
    {.Path = "scenario/world/osm/area", .Children = "", .Required = "kind"},
    {.Path = "scenario/render", .Children = "keep output stage"},
    {.Path = "scenario/render/keep", .Children = "", .Required = "name"},
    {.Path = "scenario/render/output", .Children = "", .Required = "name"},
    {.Path = "scenario/render/stage", .Children = "", .Required = "name"},
    {.Path = "scenario/lighting", .Children = "key environment"},
    {.Path = "scenario/lighting/key", .Children = ""},
    {.Path = "scenario/lighting/environment", .Children = ""},
    {.Path = "scenario/providers", .Children = "provider"},
    {.Path = "scenario/providers/provider", .Children = "", .Required = "kind"},
    {.Path = "scenario/generators", .Children = "generator"},
    {.Path = "scenario/generators/generator", .Children = "set", .Required = "kind"},
    {.Path = "scenario/generators/generator/set", .Children = ""},
    {.Path = "scenario/compositors", .Children = "compositor"},
    {.Path = "scenario/compositors/compositor", .Children = "", .Required = "kind"},
    {.Path = "scenario/assets", .Children = "asset"},
    {.Path = "scenario/assets/asset", .Children = "wears", .Required = "uri"},
    {.Path = "scenario/assets/asset/wears", .Children = "row", .Required = ""},
    {.Path = "scenario/assets/asset/wears/row", .Children = "", .Required = ""},
    {.Path = "scenario/placements", .Children = "place"},
    {.Path = "scenario/placements/place", .Children = "", .Required = "asset"},
    {.Path = "scenario/surfaces", .Children = "surface"},
    {.Path = "scenario/surfaces/surface", .Children = "", .Required = "document"},
    {.Path = "scenario/kinds", .Children = "kind"},
    {.Path = "scenario/kinds/kind", .Children = "may has mind", .Required = "name"},
    {.Path = "scenario/kinds/kind/mind", .Children = ""},
    {.Path = "scenario/kinds/kind/may", .Children = "", .Required = "do"},
    {.Path = "scenario/kinds/kind/has", .Children = "", .Required = "name"},
    {.Path = "scenario/instances", .Children = "instance"},
    {.Path = "scenario/instances/instance", .Children = "has holds", .Required = "of"},
    {.Path = "scenario/instances/instance/has", .Children = "", .Required = "name"},
    {.Path = "scenario/instances/instance/holds", .Children = "", .Required = "what"},
    {.Path = "scenario/regions", .Children = "region door"},
    {.Path = "scenario/regions/region", .Children = "uses"},
    {.Path = "scenario/regions/region/uses", .Children = "", .Required = "what"},
    {.Path = "scenario/regions/door", .Children = "", .Required = "from to"},
    {.Path = "scenario/volumes", .Children = "volume"},
    {.Path = "scenario/volumes/volume", .Children = "", .Required = "fires when"},
    {.Path = "scenario/audio", .Children = "bus sound"},
    {.Path = "scenario/audio/bus", .Children = "room voice", .Required = "id"},
    {.Path = "scenario/audio/bus/room", .Children = ""},
    {.Path = "scenario/audio/bus/voice", .Children = "from", .Required = "id"},
    {.Path = "scenario/audio/bus/voice/from", .Children = "", .Required = "id"},
    {.Path = "scenario/audio/sound", .Children = "", .Required = "id uri"},
    {.Path = "scenario/tables", .Children = "table"},
    {.Path = "scenario/tables/table", .Children = "column row"},
    {.Path = "scenario/tables/table/column", .Children = "", .Required = "name"},
    {.Path = "scenario/tables/table/row", .Children = "cell"},
    {.Path = "scenario/tables/table/row/cell", .Children = ""},
    {.Path = "scenario/events", .Children = "event"},
    {.Path = "scenario/events/event", .Children = "carries", .Required = "name"},
    {.Path = "scenario/events/event/carries", .Children = "", .Required = "what"},
    {.Path = "scenario/views", .Children = "view"},
    {.Path = "scenario/views/view", .Children = "at lookAt up", .Required = "id"},
    {.Path = "scenario/views/view/at", .Children = ""},
    {.Path = "scenario/views/view/lookAt", .Children = ""},
    {.Path = "scenario/views/view/up", .Children = ""},
    {.Path = "scenario/player", .Children = ""},
    {.Path = "scenario/body", .Children = "at centreOfMass inertia contact actuator aero slot"},
    {.Path = "scenario/body/at", .Children = ""},
    {.Path = "scenario/body/centreOfMass", .Children = ""},
    {.Path = "scenario/body/inertia", .Children = ""},
    {.Path = "scenario/body/contact", .Children = ""},
    {.Path = "scenario/body/actuator", .Children = ""},
    {.Path = "scenario/body/aero", .Children = ""},
    {.Path = "scenario/body/slot", .Children = ""},
    {.Path = "scenario/drive",
     .Children = "",
     .Required = "",
     .Allowed = "fromLat fromLon toLat toLon"},
    {.Path = "scenario/physics", .Children = ""},
    {.Path = "scenario/clock", .Children = ""},
    {.Path = "scenario/input", .Children = "bind"},
    {.Path = "scenario/input/bind", .Children = "", .Required = "event action"},
    {.Path = "scenario/state", .Children = "persist"},
    {.Path = "scenario/state/persist", .Children = "", .Required = "what"},
};

bool Names(std::string_view list, std::string_view wanted) {
  while (!list.empty()) {
    const size_t stop = list.find(' ');
    if (list.substr(0, stop) == wanted) { return true; }
    if (stop == std::string_view::npos) { return false; }
    list.remove_prefix(stop + 1);
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
  for (const char *at = known->Required; *at != 0;) {
    const char *end = at;
    while (*end != 0 && *end != ' ') { ++end; }
    const std::string wanted(at, end);
    if (!node.Spelt(wanted.c_str())) {
      error = "<" + node.Name() + "> declares no '" + wanted +
              "', and without it the element names nothing";
      return false;
    }
    at = *end == ' ' ? end + 1 : end;
  }
  if (*known->Allowed != 0) {
    for (size_t at = 0; at < node.AttributeCount(); ++at) {
      const std::string spelt = node.AttributeAt(at);
      if (Names(known->Allowed, spelt)) { continue; }
      error = "<" + node.Name() + "> spells '" + spelt +
              "', and the attributes it may carry are: " + known->Allowed;
      return false;
    }
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

void ReadVector(const Xml::Ref &from, const char *x, const char *y, const char *z, Vec3 &into) {
  into[0] = from.Num(x, into[0]);
  into[1] = from.Num(y, into[1]);
  into[2] = from.Num(z, into[2]);
}

void ReadStanding(const Xml::Ref &from, Standing &into) {
  if (from.Spelt("lat") || from.Spelt("lon")) {
    into.GlobeAnchor = true;
    into.Geodetic.LatitudeDeg = from.Num("lat", into.Geodetic.LatitudeDeg);
    into.Geodetic.LongitudeDeg = from.Num("lon", into.Geodetic.LongitudeDeg);
    into.Geodetic.HeightM = from.Num("heightM", into.Geodetic.HeightM);
    into.SamplesHeight = std::string(from.Attr("samplesHeight", "no")) == "yes";
    into.BearingDeg = from.Num("bearingDeg", into.BearingDeg);
    into.PitchDeg = from.Num("pitchDeg", into.PitchDeg);
  }
  ReadVector(from, "x", "y", "z", into.AtM);
  into.Facing.X = from.Num("qx", into.Facing.X);
  into.Facing.Y = from.Num("qy", into.Facing.Y);
  into.Facing.Z = from.Num("qz", into.Facing.Z);
  into.Facing.W = from.Num("qw", into.Facing.W);
  const double evenly = from.Num("scale", 0.0);
  if (evenly > 0.0) { into.ScaleXyz[0] = into.ScaleXyz[1] = into.ScaleXyz[2] = evenly; }
}

void ReadWorld(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Ground.Declared = true;
  into.Ground.Origin.LatitudeDeg = from.Num("lat", into.Ground.Origin.LatitudeDeg);
  into.Ground.Origin.LongitudeDeg = from.Num("lon", into.Ground.Origin.LongitudeDeg);
  into.Ground.Origin.RadiusM = from.Num("radiusM", into.Ground.Origin.RadiusM);
  into.Ground.GravityMs2 = from.Num("gravityMs2", into.Ground.GravityMs2);
  into.Ground.AirDensityKgM3 = from.Num("airDensityKgM3", into.Ground.AirDensityKgM3);
  into.Ground.Sky.WindDeg = from.Num("windDeg", into.Ground.Sky.WindDeg);
  into.Ground.Sky.WindMs = from.Num("windMs", into.Ground.Sky.WindMs);
  into.Ground.Sky.CloudCover = from.Num("cloudCover", into.Ground.Sky.CloudCover);
  into.Ground.Sky.CloudLow = from.Num("cloudLow", into.Ground.Sky.CloudLow);
  into.Ground.Sky.CloudMid = from.Num("cloudMid", into.Ground.Sky.CloudMid);
  into.Ground.Sky.CloudHigh = from.Num("cloudHigh", into.Ground.Sky.CloudHigh);
  into.Ground.Sky.CloudBaseAglM = from.Num("cloudBaseAglM", into.Ground.Sky.CloudBaseAglM);
  into.Ground.PatienceS = from.Num("patienceS", into.Ground.PatienceS);
  into.Ground.SightM = from.Num("sightM", into.Ground.SightM);
  const Xml::Ref relief = from.Child("relief");
  if (relief.Valid()) {
    into.Ground.Shape.Kind = relief.Attr("kind", into.Ground.Shape.Kind.c_str());
    into.Ground.Shape.AmplitudeM = relief.Num("amplitudeM", into.Ground.Shape.AmplitudeM);
    into.Ground.Shape.WavelengthM = relief.Num("wavelengthM", into.Ground.Shape.WavelengthM);
    into.Ground.Shape.Gradient = relief.Num("gradient", into.Ground.Shape.Gradient);
    into.Ground.Shape.BearingDeg = relief.Num("bearingDeg", into.Ground.Shape.BearingDeg);
    into.Ground.Shape.Seed =
        static_cast<uint64_t>(relief.Num("seed", static_cast<double>(into.Ground.Shape.Seed)));
  }
  const Xml::Ref osm = from.Child("osm");
  if (osm.Valid()) {
    const auto take = [&into](const Xml::Ref &node, bool area) {
      Structure made;
      made.Kind = node.Attr("kind", "");
      made.WidthM = node.Num("widthM", 0.0);
      made.HeightM = node.Num("heightM", 0.0);
      made.Area = area;
      made.Bridge = std::string(node.Attr("bridge", "no")) == "yes";
      made.Tunnel = std::string(node.Attr("tunnel", "no")) == "yes";
      made.Level = static_cast<int>(node.Num("level", 0.0));
      const std::string said = node.Attr("points", "");
      size_t at = 0;
      while (at < said.size()) {
        const size_t comma = said.find(',', at);
        if (comma == std::string::npos) { break; }
        size_t space = said.find(' ', comma);
        if (space == std::string::npos) { space = said.size(); }
        made.LatLon.push_back(std::strtod(said.c_str() + at, nullptr));
        made.LatLon.push_back(std::strtod(said.c_str() + comma + 1, nullptr));
        at = space + 1;
      }
      if (made.LatLon.size() >= 4) { into.Ground.Osm.push_back(std::move(made)); }
    };
    for (const Xml::Ref one : osm.Children("way")) { take(one, false); }
    for (const Xml::Ref one : osm.Children("area")) { take(one, true); }
  }
}

void ReadRender(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Render.Declared = true;
  if (Declares(from, "output")) { into.Render.Outputs.clear(); }
  if (Declares(from, "stage")) { into.Render.Stages.clear(); }
  into.Render.Frame.WidthPx = static_cast<int>(from.Int("widthPx", into.Render.Frame.WidthPx));
  into.Render.Frame.HeightPx = static_cast<int>(from.Int("heightPx", into.Render.Frame.HeightPx));
  into.Render.Fps = from.Num("fps", into.Render.Fps);
  into.Render.Fill = from.Num("fill", into.Render.Fill);
  into.Render.Audits = std::string(from.Attr("audits", "no")) == "yes";
  into.Render.OrbitDegPerFrame = from.Num("orbitDegPerFrame", into.Render.OrbitDegPerFrame);
  into.Render.Transfer = from.Attr("transfer", into.Render.Transfer.c_str());
  into.Render.Exposure = from.Num("exposure", into.Render.Exposure);
  into.Render.Precision = from.Attr("precision", into.Render.Precision.c_str());
  for (const Xml::Ref output : from.Children("output")) {
    into.Render.Outputs.push_back(output.Attr("name"));
  }
  if (Declares(from, "keep")) { into.Render.Outputs.clear(); }
  for (const Xml::Ref keep : from.Children("keep")) {
    into.Render.Outputs.push_back(keep.Attr("name"));
  }
  for (const Xml::Ref stage : from.Children("stage")) {
    into.Render.Stages.push_back(stage.Attr("name"));
  }
}

void ReadLighting(const Xml::Ref &from, Scenario &into) {
  if (!from.Valid()) { return; }
  into.Lit.Declared = true;
  into.Lit.ShadowRadiusM = from.Num("shadowRadiusM", 0.0);
  const Xml::Ref key = from.Child("key");
  if (key.Valid()) {
    into.Lit.Key.Lux = key.Num("lux", into.Lit.Key.Lux);
    into.Lit.Key.ElevationDeg = key.Num("elevationDeg", into.Lit.Key.ElevationDeg);
    into.Lit.Key.BearingDeg = key.Num("bearingDeg", into.Lit.Key.BearingDeg);
  }
  const Xml::Ref environment = from.Child("environment");
  if (environment.Valid()) {
    into.Lit.IndirectLight[0] = environment.Num("r", into.Lit.IndirectLight[0]);
    into.Lit.IndirectLight[1] = environment.Num("g", into.Lit.IndirectLight[1]);
    into.Lit.IndirectLight[2] = environment.Num("b", into.Lit.IndirectLight[2]);
  }
}

} // namespace

[[nodiscard]] bool ReadSectionsOnto(const Xml::Ref &root, Scenario &into, std::string &error) {
  ReadWorld(root.Child("world"), into);
  ReadRender(root.Child("render"), into);
  ReadLighting(root.Child("lighting"), into);

  const Xml::Ref physics = root.Child("physics");
  if (physics.Valid()) {
    into.Motion.Declared = true;
    into.Motion.Dial = physics.Attr("dial", into.Motion.Dial.c_str());
    into.Motion.StepS = physics.Num("stepS", into.Motion.StepS);
    into.Motion.MostStepsInArrears = static_cast<int>(
        physics.Num("mostStepsInArrears", static_cast<double>(into.Motion.MostStepsInArrears)));
  }

  const Xml::Ref clock = root.Child("clock");
  if (clock.Valid()) {
    into.Time.Declared = true;
    into.Time.Start = clock.Attr("start", into.Time.Start.c_str());
    into.Time.Rate = clock.Num("rate", into.Time.Rate);
    into.Time.Live = std::string(clock.Attr("live", into.Time.Live ? "yes" : "no")) == "yes" ||
                     into.Time.Start.empty();
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
    into.Routed.Declared = true;
    const std::string by = drive.Attr("by");
    if (by.empty() || by == "drive") {
      into.Routed.By = Travels::Drive;
    } else if (by == "walk") {
      into.Routed.By = Travels::Walk;
    } else if (by == "fly") {
      into.Routed.By = Travels::Fly;
    } else if (by == "rail") {
      into.Routed.By = Travels::Rail;
    } else {
      error =
          "a journey travels '" + by + "', and walk, drive, fly and rail are the whole catalogue";
      return false;
    }
    into.Routed.FromLatDeg = drive.Num("fromLat", into.Routed.FromLatDeg);
    into.Routed.FromLonDeg = drive.Num("fromLon", into.Routed.FromLonDeg);
    into.Routed.ToLatDeg = drive.Num("toLat", into.Routed.ToLatDeg);
    into.Routed.ToLonDeg = drive.Num("toLon", into.Routed.ToLonDeg);
  }
  return true;
}

bool ReadScenario(const char *text, size_t length, Scenario &into, std::string &error) {
  Xml document;
  if (!document.Parse(text, length)) {
    error = document.Error();
    return false;
  }
  return ReadScenario(document, into, error);
}

bool ReadScenario(const Xml &document, Scenario &into, std::string &error) {
  into = Scenario();
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
    into.Layers.push_back(
        Layer{.Id = one.Attr("id"), .Path = one.Attr("path"), .Set = one.Attr("set")});
  }

  if (!ReadSectionsOnto(root, into, error)) { return false; }

  const Xml::Ref providers = root.Child("providers");
  for (const Xml::Ref one : providers.Children("provider")) {
    Provider made;
    made.Kind = one.Attr("kind");
    made.Pin = one.Attr("pin");
    made.Rank = static_cast<int>(one.Int("rank", 0));
    made.WhenAbsent = one.Attr("whenAbsent");
    into.Providers.push_back(made);
  }

  const Xml::Ref generators = root.Child("generators");
  for (const Xml::Ref one : generators.Children("generator")) {
    Generator made;
    made.Kind = one.Attr("kind");
    for (const Xml::Ref parameter : one.Children("set")) {
      made.Parameters.push_back(
          Setting{.Name = parameter.Attr("name"), .Value = parameter.Attr("value")});
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
    made.Clip = static_cast<int>(one.Num("clip", 0.0));
    const std::string animation = one.Attr("animation", "play");
    if (animation == "play") {
      made.Animation = AssetAnimation::Play;
    } else if (animation == "loop") {
      made.Animation = AssetAnimation::Loop;
    } else if (animation == "ignore") {
      made.Animation = AssetAnimation::Ignore;
    } else if (animation == "driven") {
      made.Animation = AssetAnimation::Driven;
    } else {
      error = "<asset> declares animation='" + animation +
              "', and the four answers are play, loop, ignore and driven";
      return false;
    }
    for (const Xml::Ref worn : one.Children("wears")) {
      SurfaceOverride said;
      said.Named = worn.Attr("named");
      said.Node = worn.Attr("node");
      said.Part = static_cast<int>(worn.Num("part", -1.0));
      said.KeepsMaps = std::string(worn.Attr("keepsMaps", "no")) == "yes";
      const Xml::Ref row = worn.Child("row");
      if (row.Valid()) {
        said.Row.BaseColour[0] =
            static_cast<float>(row.Num("r", static_cast<double>(said.Row.BaseColour[0])));
        said.Row.BaseColour[1] =
            static_cast<float>(row.Num("g", static_cast<double>(said.Row.BaseColour[1])));
        said.Row.BaseColour[2] =
            static_cast<float>(row.Num("b", static_cast<double>(said.Row.BaseColour[2])));
        said.Row.BaseColour[3] =
            static_cast<float>(row.Num("a", static_cast<double>(said.Row.BaseColour[3])));
        said.Row.Metalness =
            static_cast<float>(row.Num("metalness", static_cast<double>(said.Row.Metalness)));
        said.Row.Roughness =
            static_cast<float>(row.Num("roughness", static_cast<double>(said.Row.Roughness)));
        said.Row.Emission[0] =
            static_cast<float>(row.Num("emissionR", static_cast<double>(said.Row.Emission[0])));
        said.Row.Emission[1] =
            static_cast<float>(row.Num("emissionG", static_cast<double>(said.Row.Emission[1])));
        said.Row.Emission[2] =
            static_cast<float>(row.Num("emissionB", static_cast<double>(said.Row.Emission[2])));
        said.Row.Unlit = std::string(row.Attr("unlit", "no")) == "yes";
        said.Row.DoubleSided = std::string(row.Attr("doubleSided", "no")) == "yes";
        said.Row.CoverageCut =
            static_cast<float>(row.Num("coverageCut", static_cast<double>(said.Row.CoverageCut)));
      }
      made.Surfaces.push_back(said);
    }
    into.Assets.push_back(made);
  }

  const Xml::Ref placements = root.Child("placements");
  for (const Xml::Ref one : placements.Children("place")) {
    Placement made;
    made.Asset = one.Attr("asset");
    ReadStanding(one, made.Stands);
    into.Placements.push_back(made);
  }

  const Xml::Ref surfaces = root.Child("surfaces");
  for (const Xml::Ref one : surfaces.Children("surface")) {
    Surface made;
    made.Document = one.Attr("document");
    made.Style = one.Attr("style");
    made.Programme = one.Attr("programme");
    made.Where.LeftFrac = one.Num("leftFrac", 0.0);
    made.Where.TopFrac = one.Num("topFrac", 0.0);
    made.Where.WidthFrac = one.Num("widthFrac", 1.0);
    made.Where.HeightFrac = one.Num("heightFrac", 1.0);
    made.Z = static_cast<int>(one.Int("z", 0));
    into.Surfaces.push_back(made);
  }

  const Xml::Ref input = root.Child("input");
  if (input.Valid()) { into.WheelStepPx = input.Num("wheelStepPx", into.WheelStepPx); }
  for (const Xml::Ref one : input.Children("bind")) {
    into.Input.push_back(Binding{.Event = one.Attr("event"), .Action = one.Attr("action")});
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
      thinks.TokenBudget = static_cast<int>(mind.Int("tokenBudget", 0));
      thinks.LatencyBudgetMs = mind.Num("latencyBudgetMs", 0.0);
      thinks.Temperature = mind.Num("temperature", 0.0);
      thinks.Seed = mind.Int("seed", 0);
      made.Minds.push_back(thinks);
    }
    for (const Xml::Ref may : one.Children("may")) { made.Capabilities.push_back(may.Attr("do")); }
    for (const Xml::Ref attribute : one.Children("has")) {
      made.Attributes.push_back(
          Setting{.Name = attribute.Attr("name"), .Value = attribute.Attr("value")});
    }
    into.Kinds.push_back(made);
  }

  const Xml::Ref instances = root.Child("instances");
  into.Room = static_cast<size_t>(root.Child("scene").Num("room", 0.0));
  for (const Xml::Ref one : instances.Children("instance")) {
    Instance made;
    made.Of = one.Attr("of");
    made.Id = one.Attr("id");
    made.In = one.Attr("in");
    ReadStanding(one, made.Stands);
    for (const Xml::Ref attribute : one.Children("has")) {
      made.Attributes.push_back(
          Setting{.Name = attribute.Attr("name"), .Value = attribute.Attr("value")});
    }
    for (const Xml::Ref holds : one.Children("holds")) { made.Holds.push_back(holds.Attr("what")); }
    into.Instances.push_back(made);
  }

  const Xml::Ref regions = root.Child("regions");
  for (const Xml::Ref one : regions.Children("region")) {
    Region made;
    made.Id = one.Attr("id");
    made.Kind = one.Attr("kind");
    ReadVector(one, "x", "y", "z", made.OriginM);
    made.RadiusM = one.Num("radiusM", 0.0);
    made.Streams = one.Flag("streams", true);
    for (const Xml::Ref uses : one.Children("uses")) { made.Uses.push_back(uses.Attr("what")); }
    into.Regions.push_back(made);
  }
  for (const Xml::Ref one : regions.Children("door")) {
    Door made;
    made.Id = one.Attr("id");
    made.From = one.Attr("from");
    made.To = one.Attr("to");
    ReadVector(one, "x", "y", "z", made.AtM);
    into.Doors.push_back(made);
  }

  const Xml::Ref volumes = root.Child("volumes");
  for (const Xml::Ref one : volumes.Children("volume")) {
    Volume made;
    made.Id = one.Attr("id");
    made.In = one.Attr("in");
    made.Shape = one.Attr("shape", "box");
    ReadVector(one, "x", "y", "z", made.AtM);
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
    Bus made;
    made.Id = one.Attr("id");
    made.Into = one.Attr("into");
    made.GainDb = one.Num("gainDb", 0.0);
    const Xml::Ref room = one.Child("room");
    made.Reverberates.Declared = room.Num("secondsRt60", 0.0) > 0.0;
    made.Reverberates.SecondsRt60 = room.Num("secondsRt60", 0.0);
    made.Reverberates.Damping = room.Num("damping", 0.5);
    made.Reverberates.WetShare = room.Num("wetShare", 0.0);
    into.Buses.push_back(made);
  }
  for (const Xml::Ref one : audio.Children("sound")) {
    Sound made;
    made.Id = one.Attr("id");
    made.Uri = one.Attr("uri");
    made.Bus = one.Attr("bus");
    made.On = one.Attr("on");
    made.Streamed = one.Flag("streamed", false);
    made.Loops = one.Flag("loops", false);
    made.GainDb = one.Num("gainDb", 0.0);
    made.Heard.Positional = one.Flag("positional", false);
    const std::string falls = one.Attr("falls");
    made.Heard.By = falls == "linear"        ? Falls::Linear
                    : falls == "exponential" ? Falls::Exponential
                                             : Falls::Inverse;
    made.Heard.RefM = one.Num("refM", 1.0);
    made.Heard.MostM = one.Num("mostM", 0.0);
    made.Heard.Rolloff = one.Num("rolloff", 1.0);
    made.Heard.InnerRad = one.Num("innerRad", 0.0);
    made.Heard.OuterRad = one.Num("outerRad", 0.0);
    made.Heard.OuterGain = one.Num("outerGain", 0.0);
    made.Heard.BlockedGain = one.Num("blockedGain", 1.0);
    made.Heard.BlockedHz = one.Num("blockedHz", 0.0);
    made.SendShare = one.Num("sendShare", 0.0);
    for (const Xml::Ref unit : one.Children("voice")) {
      Voice makes;
      makes.Id = unit.Attr("id");
      const std::string does = unit.Attr("does");
      makes.Does = does == "noise"       ? Makes::Noise
                   : does == "biquad"    ? Makes::Biquad
                   : does == "delay"     ? Makes::Delay
                   : does == "gain"      ? Makes::Gain
                   : does == "shaper"    ? Makes::Shaper
                   : does == "convolver" ? Makes::Convolver
                   : does == "mix"       ? Makes::Mix
                                         : Makes::Oscillator;
      for (const Xml::Ref from : unit.Children("from")) { makes.From.push_back(from.Attr("id")); }
      for (const Xml::Ref set : unit.Children("set")) {
        makes.Parameters.push_back(Setting{.Name = set.Attr("name"), .Value = set.Attr("value")});
      }
      made.Graph.push_back(makes);
    }
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
      for (const Xml::Ref cell : row.Children("cell")) { cells.push_back(cell.Attr("value")); }
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
    if (Declares(one, "at")) {
      made.Sees.Placed = true;
      ReadStanding(one.Child("at"), made.Sees.Stands);
    }
    made.Person = one.Attr("person");
    made.DistanceM = one.Num("distanceM", 0.0);
    made.RisesBy = one.Num("risesBy", made.RisesBy);
    made.PitchLimitDeg = one.Num("pitchLimitDeg", 89.0);
    made.OffsetM[0] = one.Num("offsetX", 0.0);
    made.OffsetM[1] = one.Num("offsetY", 0.0);
    made.OffsetM[2] = one.Num("offsetZ", 0.0);
    made.Sees.FovDeg = one.Num("fovDeg", 0.0);
    made.Sees.NearM = one.Num("nearM", 0.0);
    made.Sees.FarM = one.Num("farM", 0.0);
    made.Sees.Orthographic = std::string(one.Attr("orthographic", "no")) == "yes";
    made.Sees.XMagM = one.Num("xMagM", 0.0);
    made.Sees.YMagM = one.Num("yMagM", 0.0);
    made.Sees.ApertureFStops = one.Num("apertureFStops", 0.0);
    made.Sees.ShutterS = one.Num("shutterS", 0.0);
    made.Sees.SensitivityIso = one.Num("sensitivityIso", 0.0);
    if (Declares(one, "lookAt")) {
      made.Sees.LooksAt = true;
      ReadVector(one.Child("lookAt"), "x", "y", "z", made.Sees.LookAtM);
    }
    if (Declares(one, "up")) { ReadVector(one.Child("up"), "x", "y", "z", made.Sees.UpM); }
    made.TimeScale = one.Num("timeScale", 1.0);
    into.Views.push_back(made);
  }

  for (const Xml::Ref one : root.Children("body")) {
    Body made;
    made.Name = one.Attr("name");
    made.Asset = one.Attr("asset");
    made.MassKg = one.Num("massKg", 0.0);
    made.WidthM = one.Num("widthM", 0.0);
    made.AssetSpanM = one.Num("assetSpanM", 0.0);
    made.AssetGround = one.Num("assetGround", 0.0);
    made.AssetCentreX = one.Num("assetCentreX", 0.0);
    made.AssetCentreZ = one.Num("assetCentreZ", 0.0);
    if (Declares(one, "at")) {
      const Xml::Ref where = one.Child("at");
      made.Placed = true;
      ReadStanding(where, made.Stands);
    }
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
      ReadVector(touch, "x", "y", "z", wheel.AtM);
      wheel.Strut.ReachM = touch.Num("reachM", 0.0);
      wheel.Strut.StiffnessNPerM = touch.Num("stiffnessNPerM", 0.0);
      wheel.Strut.DampingNsPerM = touch.Num("dampingNsPerM", 0.0);
      wheel.Strut.TravelM = touch.Num("travelM", 0.0);
      wheel.Strut.StopNPerM = touch.Num("stopNPerM", 0.0);
      wheel.Strut.LimitN = touch.Num("limitN", 0.0);
      wheel.Touches.Grip = touch.Num("grip", 0.0);
      wheel.Touches.LoadFalloff = touch.Num("loadFalloff", 0.0);
      wheel.Touches.RadiusM = touch.Num("radiusM", 0.0);
      wheel.Touches.CorneringNPerRad = touch.Num("corneringNPerRad", 0.0);
      wheel.Touches.RelaxationM = touch.Num("relaxationM", 0.0);
      made.Contacts.push_back(wheel);
    }
    for (const Xml::Ref acts : one.Children("actuator")) {
      Drive does;
      const std::string named = acts.Attr("does");
      if (named == "torque") {
        does.Does = Drives::Effort;
        does.Opposes = acts.Num("opposes", 0.0) != 0.0;
      } else if (named == "steer") {
        does.Does = Drives::Motion;
      } else {
        error = "a body declares a drive that does '" + named +
                "', and torque and steer are the whole catalogue -- a brake is a torque that "
                "OPPOSES, which is the one physical difference between it and a drive";
        return false;
      }
      does.PeakNm = acts.Num("peakNm", 0.0);
      does.PeakN = acts.Num("peakN", 0.0);
      does.Turns = does.PeakN == 0.0;
      does.AxisXyz[0] = acts.Num("axisX", does.Turns ? 0.0 : 0.0);
      does.AxisXyz[1] = acts.Num("axisY", does.Turns ? 1.0 : 0.0);
      does.AxisXyz[2] = acts.Num("axisZ", does.Turns ? 0.0 : -1.0);
      if (does.PeakNm != 0.0 && does.PeakN != 0.0) {
        error = "a drive applies a torque about an axis or a force along one, never both, and "
                "this one declares peakNm and peakN together";
        return false;
      }
      does.Ratio = acts.Num("ratio", 1.0);
      does.CircleM = acts.Num("circleM", 0.0);
      made.Driven.push_back(does);
    }
    const Xml::Ref aero = one.Child("aero");
    made.DragCoefficient = aero.Num("dragCoefficient", 0.0);
    made.FrontalM2 = aero.Num("frontalM2", 0.0);
    for (const Xml::Ref where : one.Children("slot")) {
      Slot advertised;
      advertised.At = where.Attr("at");
      ReadVector(where, "x", "y", "z", advertised.AtM);
      made.Slots.push_back(advertised);
    }
    into.Bodies.push_back(made);
  }

  const Xml::Ref state = root.Child("state");
  for (const Xml::Ref persist : state.Children("persist")) {
    into.State.push_back(Persisted{persist.Attr("what")});
  }

  const Xml::Unread unread = document.FirstUnread();
  if (!unread.Attribute.empty()) {
    error = "<" + unread.Path.substr(unread.Path.rfind('/') + 1) + "> at " + unread.Path +
            " carries '" + unread.Attribute +
            "', and nothing in the engine reads it -- a value that evaporates is a "
            "declaration the author believes and the engine ignores";
    return false;
  }

  return true;
}

} // namespace outshine
