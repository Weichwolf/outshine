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

struct Attribute {
  std::string Name;
  std::string Value;
};

struct Kind {
  std::string Name;
  std::string Inherits;
  std::string Asset;
  std::string Programme;
  std::vector<std::string> Capabilities;
  std::vector<Attribute> Attributes;
  double TickHz = 0.0;
};

struct Instance {
  std::string Of;
  std::string Id;
  std::string In;
  double TranslationM[3] = {0.0, 0.0, 0.0};
  double RotationXyzw[4] = {0.0, 0.0, 0.0, 1.0};
  std::vector<Attribute> Attributes;
  std::vector<std::string> Holds;
};

struct Region {
  std::string Id;
  std::string Kind;
  double OriginM[3] = {0.0, 0.0, 0.0};
  double RadiusM = 0.0;
  bool Streams = true;
  std::vector<std::string> Uses;
};

struct Door {
  std::string Id;
  std::string From;
  std::string To;
  double AtM[3] = {0.0, 0.0, 0.0};
};

struct Volume {
  std::string Id;
  std::string In;
  std::string Shape;
  double AtM[3] = {0.0, 0.0, 0.0};
  double ExtentM[3] = {0.0, 0.0, 0.0};
  std::string Fires;
  std::string When;
};

struct Sound {
  std::string Id;
  std::string Uri;
  std::string Bus;
  bool Positional = false;
  bool Loops = false;
  double GainDb = 0.0;
  double FalloffM = 0.0;
};

struct Bus {
  std::string Id;
  std::string Into;
  double GainDb = 0.0;
};

struct Table {
  std::string Id;
  std::vector<std::string> Columns;
  std::vector<std::vector<std::string>> Rows;
};

struct Event {
  std::string Name;
  std::vector<std::string> Carries;
};

struct View {
  std::string Id;
  std::string Follows;
  double OffsetM[3] = {0.0, 0.0, 0.0};
  double FovDeg = 0.0;
  double TimeScale = 1.0;
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

  std::vector<Kind> Kinds;
  std::vector<Instance> Instances;
  std::vector<Region> Regions;
  std::vector<Door> Doors;
  std::vector<Volume> Volumes;
  std::vector<Sound> Sounds;
  std::vector<Bus> Buses;
  std::vector<Table> Tables;
  std::vector<Event> Events;
  std::vector<View> Views;

  Physics Motion;
  Clock Time;
  std::vector<Binding> Input;
  std::vector<Persisted> State;

  [[nodiscard]] const Asset *Subject(void) const;
};

} // namespace outshine

#endif
