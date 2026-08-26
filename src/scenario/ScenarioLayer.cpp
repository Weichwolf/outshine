#include "ScenarioLayer.h"

#include "ScenarioRead.h"

namespace outshine {

namespace {

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
    const std::string identity = same.Identity(row);
    trace.push_back("layer '" + std::string(named) + (overrode ? "' overrode " : "' added ") +
                    what + (identity.empty() ? " (id-less)" : " '" + identity + "'"));
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
struct ByDoorEnds {
  bool operator()(const Door &a, const Door &b) const {
    return a.From == b.From && a.To == b.To;
  }
  std::string Identity(const Door &row) const { return row.From + "->" + row.To; }
};
struct BySoundUri {
  bool operator()(const Sound &a, const Sound &b) const { return a.Uri == b.Uri; }
  std::string Identity(const Sound &row) const { return row.Uri; }
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

}

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
  MergeRows(into.Providers, layer.Providers, named, "provider", ByKindField<Provider>{}, trace);
  MergeRows(into.Generators, layer.Generators, named, "generator", ByKindField<Generator>{},
            trace);
  MergeRows(into.Compositors, layer.Compositors, named, "compositor",
            ByKindField<Compositor>{}, trace);
  MergeRows(into.Regions, layer.Regions, named, "region", ByIdField<Region>{}, trace);
  MergeRows(into.Doors, layer.Doors, named, "door", ByDoorEnds{}, trace);
  MergeRows(into.Volumes, layer.Volumes, named, "volume", ByIdField<Volume>{}, trace);
  MergeRows(into.Sounds, layer.Sounds, named, "sound", BySoundUri{}, trace);
  MergeRows(into.Buses, layer.Buses, named, "bus", ByIdField<Bus>{}, trace);
  MergeRows(into.Tables, layer.Tables, named, "table", ByIdField<Table>{}, trace);
  MergeRows(into.Views, layer.Views, named, "view", ByIdField<View>{}, trace);
  MergeRows(into.Events, layer.Events, named, "event", ByEventName{}, trace);
  MergeRows(into.Surfaces, layer.Surfaces, named, "surface", BySurfaceDocument{}, trace);
  MergeRows(into.Input, layer.Input, named, "bind", ByBindingEvent{}, trace);
  MergeRows(into.State, layer.State, named, "persist", ByPersistedWhat{}, trace);

  for (const Placement &place : layer.Placements) {
    into.Placements.push_back(place);
    trace.push_back("layer '" + std::string(named) + "' added a placement of '" + place.Asset +
                    "'");
  }

  return true;
}

bool ApplyLayer(Scenario &into, const char *text, size_t size, std::string_view named,
                std::vector<std::string> &trace, std::string &error) {
  Xml document;
  if (!document.Parse(text, size)) {
    error = document.Error();
    return false;
  }
  Scenario fragment;
  if (!ReadScenario(document, fragment, error)) { return false; }
  if (!MergeLayer(into, fragment, named, trace, error)) { return false; }

  if (!fragment.Bodies.empty()) {
    into.Bodies = fragment.Bodies;
    trace.push_back("layer '" + std::string(named) + "' replaced the vehicle");
  }
  if (!fragment.Render.Stages.empty()) {
    trace.push_back("layer '" + std::string(named) + "' replaced the stage list (" +
                    std::to_string(fragment.Render.Stages.size()) + " stages -- the declared "
                    "list is the list)");
  }
  if (!fragment.Render.Outputs.empty()) {
    trace.push_back("layer '" + std::string(named) + "' replaced the output list");
  }

  struct SectionRow {
    bool DeclaredByLayer;
    const char *What;
  };
  const SectionRow sections[] = {
      {fragment.Lit.Declared, "lighting"},     {fragment.Ground.Declared, "world"},
      {fragment.Render.Declared, "render"},    {fragment.Motion.Declared, "physics"},
      {fragment.Time.Declared, "clock"},       {fragment.Played.Declared, "player"},
      {fragment.Routed.Declared, "drive"},
  };
  for (const SectionRow &section : sections) {
    if (!section.DeclaredByLayer) { continue; }
    trace.push_back("layer '" + std::string(named) + "' merged into the " + section.What +
                    " -- omitted attributes keep the base's values");
  }
  std::string why;
  if (!ReadSectionsOnto(document.Root(), into, why)) {
    error = why;
    return false;
  }
  return true;
}

}
