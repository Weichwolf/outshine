#ifndef OUTSHINE_SCENARIO_H
#define OUTSHINE_SCENARIO_H

#include <string>
#include <vector>

namespace outshine {

struct Extent {
  int WidthPx = 0;
  int HeightPx = 0;
};

struct Light {
  double Lux = 0.0;
  double ElevationDeg = 0.0;
  double BearingDeg = 0.0;
};

struct Identity {
  std::string Name;
  std::string Version;
  double Epoch = 0.0;
  double Decay = 0.0;
};

struct Layer {
  std::string Id;
  std::string Path;
};

struct World {
  bool Declared = false;
  double Lat = 0.0;
  double Lon = 0.0;
  double RadiusM = 0.0;
  double WindDeg = 0.0;
  double WindMs = 0.0;
  double CloudCover = 0.0;
};

struct Provider {
  std::string Kind;
  std::string Pin;
  int Rank = 0;
  std::string WhenAbsent;
};

struct Setting {
  std::string Name;
  std::string Value;
};

struct Generator {
  std::string Kind;
  std::vector<Setting> Parameters;
};

struct Compositor {
  std::string Kind;
  double BudgetPx = 0.0;
  bool On = true;
};

struct RenderPlan {
  Extent Frame;
  double Fps = 60.0;
  double Fill = 0.9;
  double OrbitDegPerFrame = 0.0;
  std::vector<std::string> Outputs;
  std::vector<std::string> Stages;
  std::string Transfer;
  double Exposure = 1.0;
  std::string Precision;
};

struct Lighting {
  Light Key;
  double Environment[3] = {0.0, 0.0, 0.0};
};

struct Asset {
  std::string Uri;
  std::string Digest;
  std::string Kind;
  std::string Variant;
};

struct Placement {
  std::string Asset;
  double TranslationM[3] = {0.0, 0.0, 0.0};
  double RotationXyzw[4] = {0.0, 0.0, 0.0, 1.0};
  double Scale[3] = {1.0, 1.0, 1.0};
};

struct Surface {
  std::string Document;
  std::string Style;
  std::string Programme;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
  int Z = 0;
};

struct Actor {
  std::string Kind;
  std::string Programme;
  std::vector<std::string> Capabilities;
  std::string Spawn;
  double TickHz = 0.0;
};

struct Physics {
  std::string Dial;
};

struct Clock {
  std::string Start;
  double Rate = 1.0;
};

struct Binding {
  std::string Event;
  std::string Action;
};

struct Persisted {
  std::string What;
};

struct Scenario {
  Identity Named;
  std::vector<Layer> Layers;
  World Ground;
  std::vector<Provider> Providers;
  std::vector<Generator> Generators;
  std::vector<Compositor> Compositors;
  RenderPlan Render;
  Lighting Lit;
  std::vector<Asset> Assets;
  std::vector<Placement> Placements;
  std::vector<Surface> Surfaces;
  std::vector<Actor> Actors;
  Physics Motion;
  Clock Time;
  std::vector<Binding> Input;
  std::vector<Persisted> State;

  [[nodiscard]] const Asset *Subject(void) const;
};

} // namespace outshine

#endif
