#include "ScenarioLayer.h"

namespace outshine {

namespace {

// override by the row's IDENTITY -- the same attribute the grammar's Required column names:
// a later row with a known identity REPLACES the earlier whole, an unknown identity ADDS,
// and what happened is written down, because a declaration nobody can trace is a
// declaration nobody can debug
template <class Row, class Same>
void MergeRows(std::vector<Row> &into, const std::vector<Row> &from, std::string_view named,
               const char *what, Same same, std::vector<std::string> &trace) {
  for (const Row &row : from) {
    bool overrode = false;
    for (Row &held : into) {
      if (!same(held, row)) { continue; }
      held = row;
      overrode = true;
      break;
    }
    if (!overrode) { into.push_back(row); }
    trace.push_back("layer '" + std::string(named) + (overrode ? "' overrode " : "' added ") +
                    what + " '" + same.Identity(row) + "'");
  }
}

struct ByKindName {
  bool operator()(const Kind &a, const Kind &b) const { return a.Name == b.Name; }
  std::string Identity(const Kind &row) const { return row.Name; }
};
struct ByInstanceId {
  bool operator()(const Instance &a, const Instance &b) const {
    return !a.Id.empty() && a.Id == b.Id;
  }
  std::string Identity(const Instance &row) const { return row.Id; }
};
struct ByAssetUri {
  bool operator()(const Asset &a, const Asset &b) const { return a.Uri == b.Uri; }
  std::string Identity(const Asset &row) const { return row.Uri; }
};
struct ByVehicleName {
  bool operator()(const Vehicle &a, const Vehicle &b) const { return a.Name == b.Name; }
  std::string Identity(const Vehicle &row) const { return row.Name; }
};
template <class Row>
struct ByKindField {
  bool operator()(const Row &a, const Row &b) const { return a.Kind == b.Kind; }
  std::string Identity(const Row &row) const { return row.Kind; }
};
template <class Row>
struct ByIdField {
  bool operator()(const Row &a, const Row &b) const { return !a.Id.empty() && a.Id == b.Id; }
  std::string Identity(const Row &row) const { return row.Id; }
};
struct ByEventName {
  bool operator()(const Event &a, const Event &b) const { return a.Name == b.Name; }
  std::string Identity(const Event &row) const { return row.Name; }
};
struct BySurfaceDocument {
  bool operator()(const Surface &a, const Surface &b) const { return a.Document == b.Document; }
  std::string Identity(const Surface &row) const { return row.Document; }
};
struct ByBindingEvent {
  bool operator()(const Binding &a, const Binding &b) const { return a.Event == b.Event; }
  std::string Identity(const Binding &row) const { return row.Event; }
};
struct ByPersistedWhat {
  bool operator()(const Persisted &a, const Persisted &b) const { return a.What == b.What; }
  std::string Identity(const Persisted &row) const { return row.What; }
};

} // namespace

bool LayerActive(const Layer &layer, std::string_view active) {
  if (layer.Set.empty()) { return true; }
  size_t at = 0;
  while (at < active.size()) {
    size_t end = active.find(' ', at);
    if (end == std::string_view::npos) { end = active.size(); }
    if (active.substr(at, end - at) == layer.Set) { return true; }
    at = end + 1;
  }
  return false;
}

bool MergeLayer(Scenario &into, const Scenario &layer, std::string_view named,
                std::vector<std::string> &trace, std::string &error) {
  if (!layer.Layers.empty()) {
    error = "the layer '" + std::string(named) +
            "' declares layers of its own, and a graph of overrides is a thing nobody can "
            "predict -- one level, by rule";
    return false;
  }
  MergeRows(into.Kinds, layer.Kinds, named, "kind", ByKindName{}, trace);
  MergeRows(into.Instances, layer.Instances, named, "instance", ByInstanceId{}, trace);
  MergeRows(into.Assets, layer.Assets, named, "asset", ByAssetUri{}, trace);
  MergeRows(into.Vehicles, layer.Vehicles, named, "vehicle", ByVehicleName{}, trace);
  MergeRows(into.Providers, layer.Providers, named, "provider", ByKindField<Provider>{}, trace);
  MergeRows(into.Generators, layer.Generators, named, "generator", ByKindField<Generator>{},
            trace);
  MergeRows(into.Compositors, layer.Compositors, named, "compositor",
            ByKindField<Compositor>{}, trace);
  MergeRows(into.Regions, layer.Regions, named, "region", ByIdField<Region>{}, trace);
  MergeRows(into.Doors, layer.Doors, named, "door", ByIdField<Door>{}, trace);
  MergeRows(into.Volumes, layer.Volumes, named, "volume", ByIdField<Volume>{}, trace);
  MergeRows(into.Sounds, layer.Sounds, named, "sound", ByIdField<Sound>{}, trace);
  MergeRows(into.Buses, layer.Buses, named, "bus", ByIdField<Bus>{}, trace);
  MergeRows(into.Tables, layer.Tables, named, "table", ByIdField<Table>{}, trace);
  MergeRows(into.Views, layer.Views, named, "view", ByIdField<View>{}, trace);
  MergeRows(into.Events, layer.Events, named, "event", ByEventName{}, trace);
  MergeRows(into.Surfaces, layer.Surfaces, named, "surface", BySurfaceDocument{}, trace);
  MergeRows(into.Input, layer.Input, named, "bind", ByBindingEvent{}, trace);
  MergeRows(into.State, layer.State, named, "persist", ByPersistedWhat{}, trace);

  // a place carries no identity, so a layer's placements always ADD -- overriding one is
  // unspellable, and that is written here rather than left to be discovered
  for (const Placement &place : layer.Placements) {
    into.Placements.push_back(place);
    trace.push_back("layer '" + std::string(named) + "' added a placement of '" + place.Asset +
                    "'");
  }

  // the singleton sections replace WHOLESALE when the layer declares them -- the winter mod
  // that dims the light is the canonical mod, and it works by declaring <lighting>
  if (layer.Lit.Declared) {
    into.Lit = layer.Lit;
    trace.push_back("layer '" + std::string(named) + "' replaced the lighting");
  }
  if (layer.Ground.Declared) {
    into.Ground = layer.Ground;
    trace.push_back("layer '" + std::string(named) + "' replaced the world");
  }
  if (layer.Render.Declared) {
    into.Render = layer.Render;
    trace.push_back("layer '" + std::string(named) + "' replaced the render declaration");
  }
  if (layer.Motion.Declared) {
    into.Motion = layer.Motion;
    trace.push_back("layer '" + std::string(named) + "' replaced the physics dial");
  }
  if (layer.Time.Declared) {
    into.Time = layer.Time;
    trace.push_back("layer '" + std::string(named) + "' replaced the clock");
  }
  if (layer.Driven.Declared) {
    into.Driven = layer.Driven;
    trace.push_back("layer '" + std::string(named) + "' replaced the drive");
  }
  if (!layer.Played.Is.empty()) {
    into.Played = layer.Played;
    trace.push_back("layer '" + std::string(named) + "' replaced the player");
  }
  return true;
}

} // namespace outshine
