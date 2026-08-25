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
  std::string Active;
};

struct Layer {
  std::string Id;
  std::string Path;
  std::string Set;
};

struct WorldSettings {
  bool Declared = false;
  double Lat = 0.0;
  double Lon = 0.0;
  double RadiusM = 6371008.8;
  double GravityMs2 = 9.80665;
  double AirDensityKgM3 = 1.2250;
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

struct Patch {
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;

  [[nodiscard]] bool Whole(void) const {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

struct RenderPlan {
  bool Declared = false;
  Extent Frame;
  Patch Picture;
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
  bool Declared = false;
  Light Key;
  double Environment[3] = {0.0, 0.0, 0.0};
};

enum class AssetAnimation { Play, Ignore, Driven };

struct Asset {
  std::string Uri;
  std::string Digest;
  std::string Kind;
  std::string Variant;
  AssetAnimation Animation = AssetAnimation::Play;
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
  Patch Where;
  int Z = 0;
};

struct Attribute {
  std::string Name;
  std::string Value;
};

struct Mind {
  std::string Tier;
  std::string Uses;
  std::string Programme;
  std::string Prompt;
  std::string Model;
  std::string Meanwhile;
  double Hz = 0.0;
  double EverySeconds = 0.0;
  long long StepBudget = 0;
  int TokenBudget = 0;
  double LatencyBudgetMs = 0.0;
  double Temperature = 0.0;
  long long Seed = 0;
};

struct Kind {
  std::string Name;
  std::string Inherits;
  std::string Asset;
  std::vector<Mind> Minds;
  std::vector<std::string> Capabilities;
  std::vector<Attribute> Attributes;
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
  double DwellS = 0.0;
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
  std::vector<bool> Types;
  std::vector<std::vector<std::string>> Rows;
};

struct Event {
  std::string Name;
  std::vector<std::string> Carries;
};

struct View {
  std::string Id;
  std::string Follows;
  std::string Person;
  double OffsetM[3] = {0.0, 0.0, 0.0};
  double DistanceM = 0.0;
  double PitchLimitDeg = 89.0;
  double FovDeg = 0.0;
  double TimeScale = 1.0;
};

struct Contact {
  std::string At;
  double AtM[3] = {0.0, 0.0, 0.0};
  double ReachM = 0.0;
  double StiffnessNPerM = 0.0;
  double DampingNsPerM = 0.0;
  double TravelM = 0.0;
  double StopNPerM = 0.0;
  double LimitN = 0.0;
};

struct Vehicle {
  std::string Name;
  std::string Asset;
  double MassKg = 0.0;
  double WidthM = 0.0;
  double WheelbaseM = 0.0;

  double AssetWheelbase = 0.0;

  double AssetGround = 0.0;
  double AssetCentreX = 0.0, AssetCentreZ = 0.0;
  double CentreOfMassM[3] = {0.0, 0.0, 0.0};
  double InertiaKgM2[3] = {0.0, 0.0, 0.0};
  std::vector<Contact> Contacts;
  double Grip = 0.0;
  double TyreRadiusM = 0.0;
  double TurningCircleM = 0.0;
  double TrackM = 0.0;
  double CorneringNPerRad = 0.0;
  double RelaxationM = 0.0;
  double LoadFalloff = 0.0;
  double PeakTorqueNm = 0.0;
  double FinalDrive = 0.0;
  double BrakeTorqueNm = 0.0;
  double DragCoefficient = 0.0;
  double FrontalM2 = 0.0;
  std::string SeatAt;
  double SeatM[3] = {0.0, 0.0, 0.0};
};

struct Drive {
  bool Declared = false;
  double FromLatDeg = 0.0;
  double FromLonDeg = 0.0;
  double ToLatDeg = 0.0;
  double ToLonDeg = 0.0;
};

struct Player {
  bool Declared = false;
  std::string Is;
  std::string Starts;
  std::string View;
  double EyeHeightM = 1.7;
  double WalkMs = 1.4;
  double RunMs = 4.5;
};

struct PhysicsSettings {
  bool Declared = false;
  std::string Dial;
};

struct Clock {
  bool Declared = false;
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
  WorldSettings Ground;
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
  std::vector<Vehicle> Vehicles;
  Player Played;
  Drive Driven;

  PhysicsSettings Motion;
  Clock Time;
  std::vector<Binding> Input;
  std::vector<Persisted> State;

  [[nodiscard]] const Asset *Subject(void) const;
};

}

#endif
