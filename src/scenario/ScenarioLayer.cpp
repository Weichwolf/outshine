#include "ScenarioLayer.h"

#include "ScenarioRead.h"
#include <array>
#include <vector>
#include <string_view>
#include <string>
#include <cstddef>

namespace outshine {

namespace {

template <class Row, class Same>
void MergeRows(std::vector<Row> &into,
               const std::vector<Row> &from,
               std::string_view named,
               const char *what,
               Same same,
               std::vector<std::string> &trace) {
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
  bool operator()(const Scenario::Kind &a, const Scenario::Kind &b) const {
    return a.Name == b.Name;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Kind &row) { return row.Name; }
};

struct ByInstanceId {
  bool operator()(const Scenario::Instance &a, const Scenario::Instance &b) const {
    return !a.Id.empty() && a.Id == b.Id;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Instance &row) { return row.Id; }
};

struct ByAssetUri {
  bool operator()(const Scenario::Asset &a, const Scenario::Asset &b) const {
    return a.Uri == b.Uri;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Asset &row) { return row.Uri; }
};

template <class Row> struct ByKindField {
  bool operator()(const Row &a, const Row &b) const { return a.Kind == b.Kind; }

  [[nodiscard]] std::string Identity(const Row &row) const { return row.Kind; }
};

template <class Row> struct ByIdField {
  bool operator()(const Row &a, const Row &b) const { return !a.Id.empty() && a.Id == b.Id; }

  [[nodiscard]] std::string Identity(const Row &row) const { return row.Id; }
};

struct ByDoorEnds {
  bool operator()(const Scenario::Door &a, const Scenario::Door &b) const {
    return a.From == b.From && a.To == b.To;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Door &row) {
    return row.From + "->" + row.To;
  }
};

struct BySoundUri {
  bool operator()(const Scenario::Sound &a, const Scenario::Sound &b) const {
    return a.Uri == b.Uri;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Sound &row) { return row.Uri; }
};

struct ByEventName {
  bool operator()(const Scenario::Event &a, const Scenario::Event &b) const {
    return a.Name == b.Name;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Event &row) { return row.Name; }
};

struct BySurfaceDocument {
  bool operator()(const Scenario::Surface &a, const Scenario::Surface &b) const {
    return a.Document == b.Document;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Surface &row) { return row.Document; }
};

struct ByBindingEvent {
  bool operator()(const Scenario::Binding &a, const Scenario::Binding &b) const {
    return a.Event == b.Event;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Binding &row) { return row.Event; }
};

struct ByPersistedWhat {
  bool operator()(const Scenario::Persisted &a, const Scenario::Persisted &b) const {
    return a.What == b.What;
  }

  [[nodiscard]] static std::string Identity(const Scenario::Persisted &row) { return row.What; }
};

} // namespace

bool LayerActive(const Scenario::Layer &layer, std::string_view active) {
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

bool MergeLayer(Scenario::Document &into,
                const Scenario::Document &layer,
                std::string_view named,
                std::vector<std::string> &trace,
                std::string &error) {
  if (!layer.Layers.empty()) {
    error = "the layer '" + std::string(named) +
            "' declares layers of its own, and a graph of overrides is a thing nobody can "
            "predict -- one level, by rule";
    return false;
  }
  MergeRows(into.Kinds, layer.Kinds, named, "kind", ByKindName{}, trace);
  MergeRows(into.Instances, layer.Instances, named, "instance", ByInstanceId{}, trace);
  MergeRows(into.Assets, layer.Assets, named, "asset", ByAssetUri{}, trace);
  MergeRows(
      into.Providers, layer.Providers, named, "provider", ByKindField<Scenario::Provider>{}, trace);
  MergeRows(into.Generators,
            layer.Generators,
            named,
            "generator",
            ByKindField<Scenario::Generating>{},
            trace);
  MergeRows(into.Compositors,
            layer.Compositors,
            named,
            "compositor",
            ByKindField<Scenario::Compositor>{},
            trace);
  MergeRows(into.Regions, layer.Regions, named, "region", ByIdField<Scenario::Region>{}, trace);
  MergeRows(into.Doors, layer.Doors, named, "door", ByDoorEnds{}, trace);
  MergeRows(into.Volumes, layer.Volumes, named, "volume", ByIdField<Scenario::Volume>{}, trace);
  MergeRows(into.Sounds, layer.Sounds, named, "sound", BySoundUri{}, trace);
  MergeRows(into.Buses, layer.Buses, named, "bus", ByIdField<Scenario::Bus>{}, trace);
  MergeRows(into.Tables, layer.Tables, named, "table", ByIdField<Scenario::Table>{}, trace);
  MergeRows(into.Views, layer.Views, named, "view", ByIdField<Scenario::View>{}, trace);
  MergeRows(into.Events, layer.Events, named, "event", ByEventName{}, trace);
  MergeRows(into.Surfaces, layer.Surfaces, named, "surface", BySurfaceDocument{}, trace);
  MergeRows(into.Input, layer.Input, named, "bind", ByBindingEvent{}, trace);
  MergeRows(into.State, layer.State, named, "persist", ByPersistedWhat{}, trace);

  for (const Scenario::Placement &place : layer.Placements) {
    into.Placements.push_back(place);
    trace.push_back("layer '" + std::string(named) + "' added a placement of '" + place.Asset +
                    "'");
  }

  return true;
}

bool ApplyLayer(Scenario::Document &into,
                const char *text,
                size_t size,
                std::string_view named,
                std::vector<std::string> &trace,
                std::string &error) {
  Xml document;
  if (!document.Parse(text, size)) {
    error = document.Error();
    return false;
  }
  Scenario::Document fragment;
  if (!ReadScenario(document, fragment, error)) { return false; }
  if (!MergeLayer(into, fragment, named, trace, error)) { return false; }

  if (!fragment.Bodies.empty()) {
    into.Bodies = fragment.Bodies;
    trace.push_back("layer '" + std::string(named) + "' replaced the body");
  }
  if (!fragment.Render.Stages.empty()) {
    trace.push_back("layer '" + std::string(named) + "' replaced the stage list (" +
                    std::to_string(fragment.Render.Stages.size()) +
                    " stages -- the declared "
                    "list is the list)");
  }
  if (!fragment.Render.Outputs.empty()) {
    trace.push_back("layer '" + std::string(named) + "' replaced the output list");
  }

  struct SectionRow {
    bool DeclaredByLayer;
    const char *What;
  };

  const std::array<SectionRow, 7> sections = {{
      {.DeclaredByLayer = fragment.Lit.Declared, .What = "lighting"},
      {.DeclaredByLayer = fragment.Ground.Declared, .What = "world"},
      {.DeclaredByLayer = fragment.Render.Declared, .What = "render"},
      {.DeclaredByLayer = fragment.Motion.Declared, .What = "physics"},
      {.DeclaredByLayer = fragment.Time.Declared, .What = "clock"},
      {.DeclaredByLayer = fragment.Played.Declared, .What = "player"},
  }};
  for (const SectionRow &section : sections) {
    if (!section.DeclaredByLayer) { continue; }
    trace.push_back("layer '" + std::string(named) + "' merged into the " + section.What +
                    " -- omitted attributes keep the base's values");
  }
  if (!ReadSectionsOnto(document.Root(), into)) {
    error = "a layer's sections were read and one of them refused, and the reader carries no "
            "reason for it -- the layer is '" +
            std::string(named) + "'";
    return false;
  }
  return true;
}

} // namespace outshine
