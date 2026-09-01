#include "ScenarioWrite.h"

#include <cstdio>
#include <string>

namespace outshine {

namespace {

void Number(std::string &into, const char *named, double how) {
  char said[64];
  std::snprintf(said, sizeof said, " %s=\"%.12g\"", named, how);
  into += said;
}

void Said(std::string &into, const char *named, const std::string &how) {
  if (how.empty()) { return; }
  into += " ";
  into += named;
  into += "=\"";
  into += how;
  into += "\"";
}

void Yes(std::string &into, const char *named, bool how) {
  into += " ";
  into += named;
  into += how ? "=\"yes\"" : "=\"no\"";
}

void StandingAs(std::string &into, const char *element, const outshine::Standing &stands) {
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

} // namespace

std::string WriteScenario(const Scenario &declared) {
  std::string said = "<scenario>\n";
  if (declared.Ground.Declared) {
    said += "  <world";
    Number(said, "lat", declared.Ground.Origin.LatitudeDeg);
    Number(said, "lon", declared.Ground.Origin.LongitudeDeg);
    Number(said, "patienceS", declared.Ground.PatienceS);
    Number(said, "sightM", declared.Ground.SightM);
    if (declared.Ground.Shape.Kind.empty() && declared.Ground.Osm.empty()) {
      said += "/>\n";
    } else {
      said += ">\n    ";
      said += "<relief";
      Said(said, "kind", declared.Ground.Shape.Kind);
      Number(said, "amplitudeM", declared.Ground.Shape.AmplitudeM);
      Number(said, "wavelengthM", declared.Ground.Shape.WavelengthM);
      Number(said, "gradient", declared.Ground.Shape.Gradient);
      Number(said, "bearingDeg", declared.Ground.Shape.BearingDeg);
      Number(said, "seed", static_cast<double>(declared.Ground.Shape.Seed));
      said += "/>\n";
      if (!declared.Ground.Osm.empty()) {
        said += "    ";
        said += "<osm>\n";
      }
      for (const Structure &one : declared.Ground.Osm) {
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
          if (!shape.empty()) { shape += " "; }
          shape += std::to_string(one.LatLon[at]) + "," + std::to_string(one.LatLon[at + 1]);
        }
        Said(said, "points", shape);
        said += "/>\n";
      }
      if (!declared.Ground.Osm.empty()) { said += "    </osm>\n"; }
      said += "  </world>\n";
    }
  }
  if (declared.Render.Declared) {
    said += "  <render";
    Number(said, "widthPx", declared.Render.Frame.WidthPx);
    Number(said, "heightPx", declared.Render.Frame.HeightPx);
    Number(said, "fps", declared.Render.Fps);
    Number(said, "fill", declared.Render.Fill);
    Yes(said, "audits", declared.Render.Audits);
    said += "/>\n";
  }
  if (declared.Time.Declared) {
    said += "  <clock";
    Said(said, "start", declared.Time.Start);
    Number(said, "rate", declared.Time.Rate);
    Yes(said, "live", declared.Time.Live);
    said += "/>\n";
  }
  if (declared.Lit.Declared) {
    said += "  <lighting>\n    <key";
    Number(said, "lux", declared.Lit.Key.Lux);
    Number(said, "elevationDeg", declared.Lit.Key.ElevationDeg);
    Number(said, "bearingDeg", declared.Lit.Key.BearingDeg);
    said += "/>\n  </lighting>\n";
  }
  if (!declared.Assets.empty()) {
    said += "  <assets>\n";
    for (const Asset &one : declared.Assets) {
      said += "    <asset";
      Said(said, "uri", one.Uri);
      Said(said, "kind", one.Kind);
      said += "/>\n";
    }
    said += "  </assets>\n";
  }
  if (!declared.Views.empty()) {
    said += "  <views>\n";
    for (const View &one : declared.Views) {
      said += "    <view";
      Said(said, "id", one.Id);
      Said(said, "person", one.Person);
      Said(said, "follows", one.Follows);
      Number(said, "fovDeg", one.Sees.FovDeg);
      said += ">\n";
      if (one.Sees.Placed || one.Sees.Stands.GlobeAnchor) {
        StandingAs(said, "at", one.Sees.Stands);
      }
      said += "    </view>\n";
    }
    said += "  </views>\n";
  }
  return said + "</scenario>\n";
}

} // namespace outshine
