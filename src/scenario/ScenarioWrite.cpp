#include <span>
#include "ScenarioWrite.h"
#include "Tables.h"
#include "CompositorValidation.h"
#include "WeatherValidation.h"
#include "PlayerValidation.h"
#include "WorldValidation.h"
#include <utility>

#include <expected>
#include <format>
#include <cstddef>
#include <string>

namespace outshine {

namespace {

void Number(std::string &into, const char *named, double how) {
  into += std::format(" {}=\"{}\"", named, how);
}

void Said(std::string &into, const char *named, const std::string &how, bool writeEmpty = false) {
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

void StandingAs(std::string &into,
                const char *element,
                const outshine::Scenario::Standing &stands) {
  into += "    <";
  into += element;
  if (stands.GlobeAnchor) {
    Number(into, "lat", stands.Geodetic.LatitudeDeg);
    Number(into, "lon", stands.Geodetic.LongitudeDeg);
    Number(into, "heightM", stands.Geodetic.HeightM);
    Yes(into, "samplesHeight", stands.SamplesHeight);
    Number(into, "bearingDeg", stands.BearingDeg);
    Number(into, "pitchDeg", stands.PitchDeg);
  } else {
    Number(into, "x", stands.AtM[0]);
    Number(into, "y", stands.AtM[1]);
    Number(into, "z", stands.AtM[2]);
    Number(into, "qx", stands.Facing.X);
    Number(into, "qy", stands.Facing.Y);
    Number(into, "qz", stands.Facing.Z);
    Number(into, "qw", stands.Facing.W);
  }
  into += "/>\n";
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
  WriteLighting(said, declared.Lit);
  WriteAssets(said, declared.Assets);
  if (!declared.Views.empty()) {
    said += "  <views>\n";
    for (const Scenario::View &one : declared.Views) {
      said += "    <view";
      Said(said, "id", one.Id);
      Said(said, "person", one.Person);
      Said(said, "follows", one.Follows);
      Number(said, "fovDeg", one.Sees.FovDeg);
      said += ">\n";
      if (one.Placement != Scenario::CameraPlacement::FollowEntity) {
        Scenario::Standing placement;
        placement.AtM = one.Sees.PositionM;
        placement.Facing = one.Sees.Orientation;
        placement.GlobeAnchor = one.Placement == Scenario::CameraPlacement::Geodetic;
        placement.Geodetic = one.Geographic.Geodetic;
        placement.SamplesHeight = one.Geographic.SamplesHeight;
        placement.BearingDeg = one.Geographic.BearingDeg;
        placement.PitchDeg = one.Geographic.PitchDeg;
        StandingAs(said, "at", placement);
      }
      said += "    </view>\n";
    }
    said += "  </views>\n";
  }
  WriteTables(said, declared.Tables);
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
