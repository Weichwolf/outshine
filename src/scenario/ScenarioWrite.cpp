#include <span>
#include "ScenarioWrite.h"
#include "BodyValidation.h"
#include "AudioSpellings.h"
#include "CameraSpellings.h"
#include "Tables.h"
#include "CompositorValidation.h"
#include "WeatherValidation.h"
#include "PlayerValidation.h"
#include "WorldValidation.h"
#include <utility>

#include <expected>
#include <concepts>
#include <format>
#include <cstddef>
#include <string>
#include <string_view>

namespace outshine {

namespace {

namespace Says {
constexpr auto kInvalidBodyDrive =
    "body drive requires a known mode and cannot combine peakN with peakNm";
constexpr auto kInvalidAudioEnum = "audio contains an invalid processor or attenuation enum";
constexpr auto EmptySurfaceDocument = "surface requires nonempty document text";
}

template <typename T>
  requires(std::integral<T> || std::floating_point<T>)
void Number(std::string &into, const char *named, T how) {
  into += std::format(" {}=\"{}\"", named, how);
}

void Said(std::string &into, const char *named, std::string_view how, bool writeEmpty = false) {
  if (how.empty() && !writeEmpty) { return; }
  into += ' ';
  into += named;
  into += "=\"";
  for (const char character : how) {
    switch (character) {
      case '\t': into += "&#9;"; break;
      case '\n': into += "&#10;"; break;
      case '\r': into += "&#13;"; break;
      case '&': into += "&amp;"; break;
      case '<': into += "&lt;"; break;
      case '>': into += "&gt;"; break;
      case '\"': into += "&quot;"; break;
      case '\'': into += "&apos;"; break;
      default: into += character; break;
    }
  }
  into += '\"';
}

void Yes(std::string &into, const char *named, bool how) {
  into += ' ';
  into += named;
  into += how ? "=\"yes\"" : "=\"no\"";
}

[[nodiscard]] std::expected<void, std::string>
WriteSurfaces(std::string &into, std::span<const Scenario::Surface> surfaces) {
  if (surfaces.empty()) { return {}; }
  into += "  <surfaces>\n";
  for (const auto &surface : surfaces) {
    if (surface.Document.empty()) { return std::unexpected(Says::EmptySurfaceDocument); }
    into += "    <surface";
    Said(into, "document", surface.Document);
    Said(into, "style", surface.Style);
    Said(into, "programme", surface.Programme);
    Number(into, "leftFrac", surface.Where.LeftFrac);
    Number(into, "topFrac", surface.Where.TopFrac);
    Number(into, "widthFrac", surface.Where.WidthFrac);
    Number(into, "heightFrac", surface.Where.HeightFrac);
    Number(into, "z", surface.Z);
    into += "/>\n";
  }
  into += "  </surfaces>\n";
  return {};
}

void WriteBus(std::string &into, const Scenario::Bus &bus) {
  into += "    <bus";
  Said(into, "id", bus.Id);
  Said(into, "into", bus.Into);
  Number(into, "gainDb", bus.GainDb);
  if (!bus.Reverberates.Declared) {
    into += "/>\n";
    return;
  }
  into += ">\n      <room";
  Number(into, "secondsRt60", bus.Reverberates.SecondsRt60);
  Number(into, "damping", bus.Reverberates.Damping);
  Number(into, "wetShare", bus.Reverberates.WetShare);
  into += "/>\n    </bus>\n";
}

[[nodiscard]] std::expected<void, std::string> WriteVoice(std::string &into,
                                                          const Scenario::Voice &voice) {
  const auto processor = SpellingOf(AudioFormat::kMakes, voice.Does, "");
  if (processor.empty()) { return std::unexpected(Says::kInvalidAudioEnum); }
  into += "      <voice";
  Said(into, "id", voice.Id);
  Said(into, "does", processor);
  into += ">\n";
  for (const auto &input : voice.From) {
    into += "        <from";
    Said(into, "id", input);
    into += "/>\n";
  }
  for (const auto &parameter : voice.Parameters) {
    into += "        <set";
    Said(into, "name", parameter.Name, true);
    Said(into, "value", parameter.Value, true);
    into += "/>\n";
  }
  into += "      </voice>\n";
  return {};
}

[[nodiscard]] std::expected<void, std::string> WriteSound(std::string &into,
                                                          const Scenario::Sound &sound) {
  const auto attenuation = SpellingOf(AudioFormat::kFalls, sound.Heard.By, "");
  if (attenuation.empty()) { return std::unexpected(Says::kInvalidAudioEnum); }
  into += "    <sound";
  Said(into, "id", sound.Id);
  Said(into, "uri", sound.Uri);
  Said(into, "bus", sound.Bus);
  Said(into, "on", sound.On);
  Yes(into, "streamed", sound.Streamed);
  Yes(into, "loops", sound.Loops);
  Yes(into, "positional", sound.Heard.Positional);
  Said(into, "falls", attenuation);
  Number(into, "gainDb", sound.GainDb);
  Number(into, "sendShare", sound.SendShare);
  Number(into, "refM", sound.Heard.RefM);
  Number(into, "mostM", sound.Heard.MostM);
  Number(into, "rolloff", sound.Heard.Rolloff);
  Number(into, "innerRad", sound.Heard.InnerRad);
  Number(into, "outerRad", sound.Heard.OuterRad);
  Number(into, "outerGain", sound.Heard.OuterGain);
  Number(into, "blockedGain", sound.Heard.BlockedGain);
  Number(into, "blockedHz", sound.Heard.BlockedHz);
  into += ">\n";
  for (const auto &voice : sound.Graph) {
    if (auto result = WriteVoice(into, voice); !result) { return result; }
  }
  into += "    </sound>\n";
  return {};
}

[[nodiscard]] std::expected<void, std::string> WriteAudio(std::string &into,
                                                          const Scenario::Document &document) {
  if (document.Buses.empty() && document.Sounds.empty()) { return {}; }
  into += "  <audio>\n";
  for (const auto &bus : document.Buses) { WriteBus(into, bus); }
  for (const auto &sound : document.Sounds) {
    if (auto result = WriteSound(into, sound); !result) { return result; }
  }
  into += "  </audio>\n";
  return {};
}

void WritePlayer(std::string &into, const Scenario::Player &player) {
  const Scenario::Player defaults;
  bool present =
      player.Declared || !player.Is.empty() || !player.Starts.empty() || !player.View.empty();
  for (const auto &field : kPlayerFields) {
    present = present || player.*field.Member != defaults.*field.Member;
  }
  if (!present) { return; }
  into += "  <player";
  Said(into, "is", player.Is);
  Said(into, "starts", player.Starts);
  Said(into, "view", player.View);
  for (const auto &field : kPlayerFields) { Number(into, field.Name, player.*field.Member); }
  into += "/>\n";
}

void WriteStandingAttributes(std::string &into, const Scenario::Standing &stands) {
  Number(into, "x", stands.AtM[0]);
  Number(into, "y", stands.AtM[1]);
  Number(into, "z", stands.AtM[2]);
  Number(into, "qx", stands.Facing.X);
  Number(into, "qy", stands.Facing.Y);
  Number(into, "qz", stands.Facing.Z);
  Number(into, "qw", stands.Facing.W);
  Number(into, "scaleX", stands.ScaleXyz[0]);
  Number(into, "scaleY", stands.ScaleXyz[1]);
  Number(into, "scaleZ", stands.ScaleXyz[2]);
  if (stands.GlobeAnchor) {
    Number(into, "lat", stands.Geodetic.LatitudeDeg);
    Number(into, "lon", stands.Geodetic.LongitudeDeg);
    Number(into, "heightM", stands.Geodetic.HeightM);
    Yes(into, "samplesHeight", stands.SamplesHeight);
    Number(into, "bearingDeg", stands.BearingDeg);
    Number(into, "pitchDeg", stands.PitchDeg);
  }
}

void StandingAs(std::string &into, const char *element, const Scenario::Standing &stands) {
  into += "    <";
  into += element;
  WriteStandingAttributes(into, stands);
  into += "/>\n";
}

void WriteContact(std::string &into, const Scenario::Contact &contact) {
  into += "    <contact";
  Said(into, "at", contact.At);
  Number(into, "x", contact.AtM[0]);
  Number(into, "y", contact.AtM[1]);
  Number(into, "z", contact.AtM[2]);
  Number(into, "reachM", contact.Strut.ReachM);
  Number(into, "stiffnessNPerM", contact.Strut.StiffnessNPerM);
  Number(into, "dampingNsPerM", contact.Strut.DampingNsPerM);
  Number(into, "travelM", contact.Strut.TravelM);
  Number(into, "stopNPerM", contact.Strut.StopNPerM);
  Number(into, "limitN", contact.Strut.LimitN);
  Number(into, "grip", contact.Touches.Grip);
  Number(into, "loadFalloff", contact.Touches.LoadFalloff);
  Number(into, "radiusM", contact.Touches.RadiusM);
  Number(into, "corneringNPerRad", contact.Touches.CorneringNPerRad);
  Number(into, "relaxationM", contact.Touches.RelaxationM);
  into += "/>\n";
}

[[nodiscard]] std::expected<void, std::string> WriteDrive(std::string &into,
                                                          const Scenario::Drive &drive) {
  if ((drive.Does != Scenario::Drives::Effort && drive.Does != Scenario::Drives::Motion) ||
      (drive.PeakNm != 0 && drive.PeakN != 0)) {
    return std::unexpected(Says::kInvalidBodyDrive);
  }
  into += "    <actuator";
  Said(into, "does", drive.Does == Scenario::Drives::Effort ? "torque" : "steer");
  Yes(into, "opposes", drive.Opposes);
  Yes(into, "turns", drive.Turns);
  Number(into, "axisX", drive.AxisXyz[0]);
  Number(into, "axisY", drive.AxisXyz[1]);
  Number(into, "axisZ", drive.AxisXyz[2]);
  Number(into, "peakNm", drive.PeakNm);
  Number(into, "peakN", drive.PeakN);
  Number(into, "ratio", drive.Ratio);
  Number(into, "circleM", drive.CircleM);
  into += "/>\n";
  return {};
}

void WriteBodyShape(std::string &into, const Scenario::Body &body) {
  into += "    <centreOfMass";
  Number(into, "x", body.CentreOfMassM[0]);
  Number(into, "y", body.CentreOfMassM[1]);
  Number(into, "z", body.CentreOfMassM[2]);
  into += "/>\n    <inertia";
  Number(into, "ixx", body.InertiaKgM2[0]);
  Number(into, "iyy", body.InertiaKgM2[1]);
  Number(into, "izz", body.InertiaKgM2[2]);
  into += "/>\n    <aero";
  Number(into, "dragCoefficient", body.DragCoefficient);
  Number(into, "frontalM2", body.FrontalM2);
  into += "/>\n";
  for (const auto &slot : body.Slots) {
    into += "    <slot";
    Said(into, "at", slot.At);
    Number(into, "x", slot.AtM[0]);
    Number(into, "y", slot.AtM[1]);
    Number(into, "z", slot.AtM[2]);
    into += "/>\n";
  }
}

[[nodiscard]] std::expected<void, std::string> WriteBodies(std::string &into,
                                                           std::span<const Scenario::Body> bodies) {
  if (const auto valid = ValidateBodyDynamics(bodies); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  for (const auto &body : bodies) {
    into += "  <body";
    Said(into, "name", body.Name);
    Said(into, "asset", body.Asset);
    Yes(into, "placed", body.Placed);
    Number(into, "massKg", body.MassKg);
    Number(into, "widthM", body.WidthM);
    Number(into, "assetSpanM", body.AssetSpanM);
    Number(into, "assetGround", body.AssetGround);
    Number(into, "assetCentreX", body.AssetCentreX);
    Number(into, "assetCentreZ", body.AssetCentreZ);
    into += ">\n";
    StandingAs(into, "at", body.Stands);
    WriteBodyShape(into, body);
    for (const auto &contact : body.Contacts) { WriteContact(into, contact); }
    for (const auto &drive : body.Driven) {
      if (auto result = WriteDrive(into, drive); !result) { return result; }
    }
    into += "  </body>\n";
  }
  return {};
}

void WriteCameraPose(std::string &into, const Scenario::View &view) {
  Scenario::Standing placement;
  placement.AtM = view.Sees.PositionM;
  placement.Facing = view.Sees.Orientation;
  placement.GlobeAnchor = true;
  placement.Geodetic = view.Geographic.Geodetic;
  placement.SamplesHeight = view.Geographic.SamplesHeight;
  placement.BearingDeg = view.Geographic.BearingDeg;
  placement.PitchDeg = view.Geographic.PitchDeg;
  StandingAs(into, "at", placement);
  into += "      <lookAt";
  Number(into, "x", view.Sees.LookAtM[0]);
  Number(into, "y", view.Sees.LookAtM[1]);
  Number(into, "z", view.Sees.LookAtM[2]);
  into += "/>\n      <up";
  Number(into, "x", view.Sees.UpM[0]);
  Number(into, "y", view.Sees.UpM[1]);
  Number(into, "z", view.Sees.UpM[2]);
  into += "/>\n";
}

void WriteCameraParameters(std::string &into, const Scenario::View &view) {
  Number(into, "fovDeg", view.Sees.FovDeg);
  Number(into, "nearM", view.Sees.NearM);
  Number(into, "farM", view.Sees.FarM);
  Number(into, "xMagM", view.Sees.XMagM);
  Number(into, "yMagM", view.Sees.YMagM);
  Number(into, "apertureFStops", view.Sees.ApertureFStops);
  Number(into, "shutterS", view.Sees.ShutterS);
  Number(into, "sensitivityIso", view.Sees.SensitivityIso);
  Number(into, "distanceM", view.DistanceM);
  Number(into, "risesBy", view.RisesBy);
  Number(into, "pitchLimitDeg", view.PitchLimitDeg);
  Number(into, "timeScale", view.TimeScale);
  Number(into, "leftFrac", view.Viewport.LeftFrac);
  Number(into, "topFrac", view.Viewport.TopFrac);
  Number(into, "widthFrac", view.Viewport.WidthFrac);
  Number(into, "heightFrac", view.Viewport.HeightFrac);
  Number(into, "offsetX", view.OffsetM[0]);
  Number(into, "offsetY", view.OffsetM[1]);
  Number(into, "offsetZ", view.OffsetM[2]);
  Yes(into, "orthographic", view.Sees.Orthographic);
  Yes(into, "looksAt", view.Sees.LooksAt);
}

[[nodiscard]] std::expected<void, std::string> WriteViews(std::string &into,
                                                          std::span<const Scenario::View> views) {
  if (views.empty()) { return {}; }
  into += "  <views>\n";
  for (const auto &view : views) {
    const auto mode = SpellingOf(CameraFormat::kPlacements, view.Placement, "");
    if (mode.empty()) {
      return std::unexpected(std::string(CameraFormat::Says::kInvalidPlacement));
    }
    into += "    <view";
    Said(into, "id", view.Id, true);
    Said(into, "in", view.In);
    Said(into, "person", view.Person);
    Said(into, "follows", view.Follows);
    Said(into, "placement", mode);
    WriteCameraParameters(into, view);
    into += ">\n";
    WriteCameraPose(into, view);
    into += "    </view>\n";
  }
  into += "  </views>\n";
  return {};
}

void WritePlacements(std::string &into, std::span<const Scenario::Placement> placements) {
  if (placements.empty()) { return; }
  into += "  <placements>\n";
  for (const auto &placement : placements) {
    into += "    <place";
    Said(into, "asset", placement.Asset, true);
    WriteStandingAttributes(into, placement.Stands);
    into += "/>\n";
  }
  into += "  </placements>\n";
}

void WriteAttributes(std::string &into, std::span<const Scenario::Setting> attributes) {
  for (const auto &attribute : attributes) {
    into += "      <has";
    Said(into, "name", attribute.Name, true);
    Said(into, "value", attribute.Value, true);
    into += "/>\n";
  }
}

void WriteMind(std::string &into, const Scenario::Mind &mind) {
  into += "      <mind";
  Said(into, "tier", mind.Tier);
  Said(into, "uses", mind.Uses);
  Said(into, "programme", mind.Programme);
  Said(into, "prompt", mind.Prompt);
  Said(into, "model", mind.Model);
  Said(into, "meanwhile", mind.Meanwhile);
  Number(into, "hz", mind.Hz);
  Number(into, "everyS", mind.EverySeconds);
  Number(into, "stepBudget", mind.StepBudget);
  Number(into, "tokenBudget", mind.TokenBudget);
  Number(into, "latencyBudgetMs", mind.LatencyBudgetMs);
  Number(into, "temperature", mind.Temperature);
  Number(into, "seed", mind.Seed);
  into += "/>\n";
}

void WriteKinds(std::string &into, std::span<const Scenario::Kind> kinds) {
  if (kinds.empty()) { return; }
  into += "  <kinds>\n";
  for (const auto &kind : kinds) {
    into += "    <kind";
    Said(into, "name", kind.Name, true);
    Said(into, "inherits", kind.Inherits);
    Said(into, "asset", kind.Asset);
    into += ">\n";
    for (const auto &mind : kind.Minds) { WriteMind(into, mind); }
    for (const auto &capability : kind.Capabilities) {
      into += "      <may";
      Said(into, "do", capability, true);
      into += "/>\n";
    }
    WriteAttributes(into, kind.Attributes);
    into += "    </kind>\n";
  }
  into += "  </kinds>\n";
}

void WriteInstances(std::string &into, std::span<const Scenario::Instance> instances) {
  if (instances.empty()) { return; }
  into += "  <instances>\n";
  for (const auto &instance : instances) {
    into += "    <instance";
    Said(into, "of", instance.Of, true);
    Said(into, "id", instance.Id);
    Said(into, "in", instance.In);
    WriteStandingAttributes(into, instance.Stands);
    into += ">\n";
    WriteAttributes(into, instance.Attributes);
    for (const auto &held : instance.Holds) {
      into += "      <holds";
      Said(into, "what", held, true);
      into += "/>\n";
    }
    into += "    </instance>\n";
  }
  into += "  </instances>\n";
}

const char *AnimationName(Scenario::AssetAnimation animation) {
  switch (animation) {
    case Scenario::AssetAnimation::Play: return "play";
    case Scenario::AssetAnimation::Loop: return "loop";
    case Scenario::AssetAnimation::Ignore: return "ignore";
    case Scenario::AssetAnimation::Driven: return "driven";
  }
  return "invalid";
}

void WriteSurfaceOverride(std::string &said, const Scenario::SurfaceOverride &surface) {
  said += "      <wears";
  Said(said, "named", surface.Named);
  Said(said, "node", surface.Node);
  Number(said, "part", surface.Part);
  Yes(said, "keepsMaps", surface.KeepsMaps);
  said += ">\n        <row";
  const auto &row = surface.Row;
  Number(said, "r", row.BaseColour[0]);
  Number(said, "g", row.BaseColour[1]);
  Number(said, "b", row.BaseColour[2]);
  Number(said, "a", row.BaseColour[3]);
  Number(said, "metalness", row.Metalness);
  Number(said, "roughness", row.Roughness);
  Number(said, "emissionR", row.Emission[0]);
  Number(said, "emissionG", row.Emission[1]);
  Number(said, "emissionB", row.Emission[2]);
  Yes(said, "unlit", row.Unlit);
  Yes(said, "doubleSided", row.DoubleSided);
  Number(said, "coverageCut", row.CoverageCut);
  said += "/>\n      </wears>\n";
}

void WriteAssets(std::string &said, std::span<const Scenario::Asset> assets) {
  if (assets.empty()) { return; }
  said += "  <assets>\n";
  for (const auto &asset : assets) {
    said += "    <asset";
    Said(said, "uri", asset.Uri);
    Said(said, "kind", asset.Kind);
    Said(said, "digest", asset.Digest);
    Said(said, "variant", asset.Variant);
    Said(said, "animation", AnimationName(asset.Animation));
    Number(said, "clip", asset.Clip);
    if (asset.Surfaces.empty()) {
      said += "/>\n";
      continue;
    }
    said += ">\n";
    for (const auto &surface : asset.Surfaces) { WriteSurfaceOverride(said, surface); }
    said += "    </asset>\n";
  }
  said += "  </assets>\n";
}

void WriteRelief(std::string &said, const Scenario::Relief &relief) {
  if (relief.Kind.empty()) { return; }
  said += "    ";
  said += "<relief";
  Said(said, "kind", relief.Kind);
  Number(said, "amplitudeM", relief.AmplitudeM);
  Number(said, "wavelengthM", relief.WavelengthM);
  Number(said, "gradient", relief.Gradient);
  Number(said, "bearingDeg", relief.BearingDeg);
  Number(said, "seed", static_cast<double>(relief.Seed));
  said += "/>\n";
}

void WriteOsmStructure(std::string &said, const Scenario::Structure &one) {
  said += "      ";
  said += one.Area ? "<area" : "<way";
  Said(said, "kind", one.Kind);
  if (one.WidthM > 0.0) { Number(said, "widthM", one.WidthM); }
  if (one.HeightM > 0.0) { Number(said, "heightM", one.HeightM); }
  if (one.Bridge) { Said(said, "bridge", "yes"); }
  if (one.Tunnel) { Said(said, "tunnel", "yes"); }
  if (one.Level != 0) { Number(said, "level", static_cast<double>(one.Level)); }
  std::string shape;
  for (size_t at = 0; at + 1 < one.LatLon.size(); at += 2) {
    if (!shape.empty()) { shape += ' '; }
    shape += std::format("{},{}", one.LatLon[at], one.LatLon[at + 1]);
  }
  Said(said, "points", shape);
  said += "/>\n";
}

void WriteWorld(std::string &said, const Scenario::WorldSettings &world) {
  if (!world.Declared) { return; }
  said += "  <world";
  Number(said, "lat", world.Origin.LatitudeDeg);
  Number(said, "radiusM", world.Origin.RadiusM);
  Number(said, "gravityMs2", world.GravityMs2);
  Number(said, "airDensityKgM3", world.AirDensityKgM3);
  Number(said, "lon", world.Origin.LongitudeDeg);
  Number(said, "patienceS", world.PatienceS);
  Number(said, "sightM", world.SightM);
  for (const auto &field : kWeatherFields) { Number(said, field.Name, world.Sky.*field.Member); }
  if (world.Shape.Kind.empty() && world.Osm.empty()) {
    said += "/>\n";
    return;
  }
  said += ">\n";
  WriteRelief(said, world.Shape);
  if (!world.Osm.empty()) {
    said += "    <osm>\n";
    for (const auto &feature : world.Osm) { WriteOsmStructure(said, feature); }
    said += "    </osm>\n";
  }
  said += "  </world>\n";
}

void WriteIdentity(std::string &said, const Scenario::Identity &identity) {
  said += "<scenario";
  Said(said, "name", identity.Name);
  Said(said, "version", identity.Version);
  Said(said, "active", identity.Active);
  Number(said, "epoch", identity.Epoch);
  Number(said, "decay", identity.Decay);
  said += ">\n";
}

void WriteRender(std::string &said, const Scenario::RenderPlan &render) {
  if (!render.Declared) { return; }
  said += "  <render";
  Number(said, "widthPx", render.Frame.WidthPx);
  Number(said, "heightPx", render.Frame.HeightPx);
  Number(said, "fps", render.Fps);
  Number(said, "fill", render.Fill);
  Yes(said, "audits", render.Audits);
  Number(said, "orbitDegPerFrame", render.OrbitDegPerFrame);
  Said(said, "transfer", render.Transfer);
  Number(said, "exposure", render.Exposure);
  Said(said, "precision", render.Precision);
  if (render.Outputs.empty() && render.Stages.empty()) {
    said += "/>\n";
    return;
  }
  said += ">\n";
  for (const auto &output : render.Outputs) {
    said += "    <output";
    Said(said, "name", output);
    said += "/>\n";
  }
  for (const auto &stage : render.Stages) {
    said += "    <stage";
    Said(said, "name", stage);
    said += "/>\n";
  }
  said += "  </render>\n";
}

void WriteLighting(std::string &said, const Scenario::Lighting &lighting) {
  if (!lighting.Declared) { return; }
  said += "  <lighting";
  Number(said, "shadowRadiusM", lighting.ShadowRadiusM);
  said += ">\n";
  said += "    <key";
  Number(said, "lux", lighting.Key.Lux);
  Number(said, "elevationDeg", lighting.Key.ElevationDeg);
  Number(said, "bearingDeg", lighting.Key.BearingDeg);
  said += "/>\n";
  said += "    <environment";
  Number(said, "r", lighting.IndirectLight[0]);
  Number(said, "g", lighting.IndirectLight[1]);
  Number(said, "b", lighting.IndirectLight[2]);
  said += "/>\n  </lighting>\n";
}

void WriteTables(std::string &said, std::span<const Scenario::Table> tables) {
  if (tables.empty()) { return; }
  said += "  <tables>\n";
  for (const auto &table : tables) {
    said += "    <table";
    Said(said, "id", table.Id, true);
    said += ">\n";
    for (size_t column = 0; column < table.Columns.size(); ++column) {
      said += "      <column";
      Said(said, "name", table.Columns[column], true);
      const bool numeric = column < table.Types.size() && table.Types[column];
      Said(said, "type", numeric ? "number" : "text");
      said += "/>\n";
    }
    for (const auto &row : table.Rows) {
      said += "      <row>\n";
      for (const auto &cell : row) {
        said += "        <cell";
        Said(said, "value", cell, true);
        said += "/>\n";
      }
      said += "      </row>\n";
    }
    said += "    </table>\n";
  }
  said += "  </tables>\n";
}

void WriteRegions(std::string &said,
                  std::span<const Scenario::Region> regions,
                  std::span<const Scenario::Door> doors) {
  if (regions.empty() && doors.empty()) { return; }
  said += "  <regions>\n";
  for (const auto &region : regions) {
    said += "    <region";
    Said(said, "id", region.Id);
    Said(said, "kind", region.Kind);
    Number(said, "x", region.OriginM[0]);
    Number(said, "y", region.OriginM[1]);
    Number(said, "z", region.OriginM[2]);
    Number(said, "radiusM", region.RadiusM);
    Yes(said, "streams", region.Streams);
    said += ">\n";
    for (const auto &resource : region.Uses) {
      said += "      <uses";
      Said(said, "what", resource, true);
      said += "/>\n";
    }
    said += "    </region>\n";
  }
  for (const auto &door : doors) {
    said += "    <door";
    Said(said, "id", door.Id);
    Said(said, "from", door.From, true);
    Said(said, "to", door.To, true);
    Number(said, "x", door.AtM[0]);
    Number(said, "y", door.AtM[1]);
    Number(said, "z", door.AtM[2]);
    said += "/>\n";
  }
  said += "  </regions>\n";
}

void WriteEvents(std::string &said, std::span<const Scenario::Event> events) {
  if (events.empty()) { return; }
  said += "  <events>\n";
  for (const auto &event : events) {
    said += "    <event";
    Said(said, "name", event.Name, true);
    said += ">\n";
    for (const auto &field : event.Carries) {
      said += "      <carries";
      Said(said, "what", field, true);
      said += "/>\n";
    }
    said += "    </event>\n";
  }
  said += "  </events>\n";
}

void WriteVolumes(std::string &said, std::span<const Scenario::Volume> volumes) {
  if (volumes.empty()) { return; }
  said += "  <volumes>\n";
  for (const auto &volume : volumes) {
    said += "    <volume";
    Said(said, "id", volume.Id, true);
    Said(said, "in", volume.In);
    Said(said, "shape", volume.Shape, true);
    Number(said, "x", volume.AtM[0]);
    Number(said, "y", volume.AtM[1]);
    Number(said, "z", volume.AtM[2]);
    Number(said, "extentX", volume.ExtentM[0]);
    Number(said, "extentY", volume.ExtentM[1]);
    Number(said, "extentZ", volume.ExtentM[2]);
    Said(said, "fires", volume.Fires, true);
    Said(said, "when", volume.When, true);
    Number(said, "dwellS", volume.DwellS);
    said += "/>\n";
  }
  said += "  </volumes>\n";
}

void WriteInput(std::string &said,
                std::span<const Scenario::Binding> bindings,
                double wheelStepPx) {
  said += "  <input";
  Number(said, "wheelStepPx", wheelStepPx);
  said += ">\n";
  for (const auto &binding : bindings) {
    said += "    <bind";
    Said(said, "event", binding.Event, true);
    Said(said, "action", binding.Action, true);
    said += "/>\n";
  }
  said += "  </input>\n";
}

void WritePersistence(std::string &said, std::span<const Scenario::Persisted> selections) {
  if (selections.empty()) { return; }
  said += "  <state>\n";
  for (const auto &selection : selections) {
    said += "    <persist";
    Said(said, "what", selection.What, true);
    said += "/>\n";
  }
  said += "  </state>\n";
}

void WriteProviders(std::string &said, std::span<const Scenario::Provider> providers) {
  if (providers.empty()) { return; }
  said += "  <providers>\n";
  for (const auto &provider : providers) {
    said += "    <provider";
    Said(said, "kind", provider.Kind, true);
    Said(said, "pin", provider.Pin);
    said += std::format(" rank=\"{}\"", provider.Rank);
    Said(said, "whenAbsent", provider.WhenAbsent);
    said += "/>\n";
  }
  said += "  </providers>\n";
}

void WriteCompositors(std::string &said, std::span<const Scenario::Compositor> compositors) {
  if (compositors.empty()) { return; }
  said += "  <compositors>\n";
  for (const auto &compositor : compositors) {
    said += "    <compositor";
    Said(said, "kind", compositor.Kind, true);
    Number(said, "budgetPx", compositor.BudgetPx);
    said += compositor.On ? " on=\"true\"" : " on=\"false\"";
    said += "/>\n";
  }
  said += "  </compositors>\n";
}

void WriteGenerators(std::string &said, std::span<const Scenario::Generating> generators) {
  if (generators.empty()) { return; }
  said += "  <generators>\n";
  for (const auto &generator : generators) {
    said += "    <generator";
    Said(said, "kind", generator.Kind);
    said += ">\n";
    for (const auto &parameter : generator.Parameters) {
      said += "      <set";
      Said(said, "name", parameter.Name);
      Said(said, "value", parameter.Value);
      said += "/>\n";
    }
    said += "    </generator>\n";
  }
  said += "  </generators>\n";
}

}

std::expected<std::string, std::string> WriteScenario(const Scenario::Document &declared) {
  if (const auto valid = ValidateWorld(declared.Ground); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidatePlayer(declared.Played); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidateWeather(declared.Ground.Sky); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidateCompositors(declared.Compositors); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (auto tables = TableBook::Stand(declared.Tables); !tables) {
    return std::unexpected(std::move(tables.error()));
  }
  std::string said;
  WriteIdentity(said, declared.Named);
  for (const auto &layer : declared.Layers) {
    said += "  <layer";
    Said(said, "id", layer.Id);
    Said(said, "path", layer.Path, true);
    Said(said, "set", layer.Set);
    said += "/>\n";
  }
  if (declared.Room != 0) {
    said += "  <scene";
    Number(said, "room", declared.Room);
    said += "/>\n";
  }
  if (auto surfaces = WriteSurfaces(said, declared.Surfaces); !surfaces) {
    return std::unexpected(std::move(surfaces.error()));
  }
  WriteWorld(said, declared.Ground);
  WriteRender(said, declared.Render);
  if (declared.Motion.Declared) {
    said += "  <physics";
    Said(said, "dial", declared.Motion.Dial);
    Number(said, "stepS", declared.Motion.StepS);
    Number(said, "mostStepsInArrears", declared.Motion.MostStepsInArrears);
    said += "/>\n";
  }
  if (declared.Time.Declared) {
    said += "  <clock";
    Said(said, "start", declared.Time.Start);
    Number(said, "rate", declared.Time.Rate);
    Yes(said, "live", declared.Time.Live);
    said += "/>\n";
  }
  if (auto audio = WriteAudio(said, declared); !audio) {
    return std::unexpected(std::move(audio.error()));
  }
  WriteLighting(said, declared.Lit);
  WriteAssets(said, declared.Assets);
  WritePlacements(said, declared.Placements);
  if (auto bodies = WriteBodies(said, declared.Bodies); !bodies) {
    return std::unexpected(std::move(bodies.error()));
  }
  WriteKinds(said, declared.Kinds);
  WriteInstances(said, declared.Instances);
  if (auto views = WriteViews(said, declared.Views); !views) {
    return std::unexpected(std::move(views.error()));
  }
  WriteTables(said, declared.Tables);
  WriteRegions(said, declared.Regions, declared.Doors);
  WriteEvents(said, declared.Events);
  WriteVolumes(said, declared.Volumes);
  WriteProviders(said, declared.Providers);
  WriteGenerators(said, declared.Generators);
  WriteCompositors(said, declared.Compositors);
  WritePersistence(said, declared.State);
  WritePlayer(said, declared.Played);
  WriteInput(said, declared.Input, declared.WheelStepPx);
  return said + "</scenario>\n";
}

}
