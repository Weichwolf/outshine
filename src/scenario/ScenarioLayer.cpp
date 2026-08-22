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
  return true;
}

} // namespace outshine
