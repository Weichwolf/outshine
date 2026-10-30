#ifndef OUTSHINE_TEST_CANONICALGROUNDSOURCEFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDSOURCEFIXTURE_H

#include "Check.h"
#include "GroundPortSources.h"
#include "Json.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outshine::Test::CanonicalGroundFixture {
inline constexpr auto kRoot = "test/experiments/transport/fixtures/canonical-ports/";

struct Source {
  std::shared_ptr<const World::TransportNetworkSnapshot> Snapshot;
  std::vector<Generators::GroundPortSourceProfile> Profiles;
  std::vector<uint64_t> Unknown;
  std::array<std::vector<World::TransportEdgeId>, 3> Components;
};

[[nodiscard]] inline uint64_t Word(Json::Ref value) {
  const auto raw = value.Source();
  if (value.GetKind() != Json::Kind::Number || raw.empty()) {
    CHECK(false, "captured unsigned words require a nonempty numerical token");
    return 0;
  }
  uint64_t result = 0;
  const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), result);
  CHECK(value.GetKind() == Json::Kind::Number && parsed.ec == std::errc{} &&
            parsed.ptr == raw.data() + raw.size(),
        "native source words are decoded from original decimal tokens without a double roundtrip");
  return result;
}

[[nodiscard]] inline std::expected<std::string, std::string> ReadCapture() {
  constexpr auto kMostBytes = size_t{8} * 1024 * 1024;
  constexpr size_t kChunkBytes = 65536;
  gzFile file = gzopen((std::string(kRoot) + "native.json.gz").c_str(), "rb");
  if (file == nullptr) { return std::unexpected("captured native source is unavailable"); }
  std::string bytes;
  std::array<char, kChunkBytes> chunk{};
  int count = 0;
  while ((count = gzread(file, chunk.data(), chunk.size())) > 0) {
    if (static_cast<size_t>(count) > kMostBytes - bytes.size()) {
      (void)gzclose(file);
      return std::unexpected("captured native source exceeds its byte budget");
    }
    bytes.append(chunk.data(), static_cast<size_t>(count));
  }
  const int closed = gzclose(file);
  if (count < 0 || closed != Z_OK) { return std::unexpected("native source gzip is invalid"); }
  return bytes;
}

[[nodiscard]] inline World::TransportEdge Edge(Json::Ref row) {
  using namespace World;
  const auto id = row["Id"];
  return {.Id = {.PathId = Word(id["PathId"]),
                 .SegmentOrdinal = static_cast<uint32_t>(Word(id["SegmentOrdinal"])),
                 .Direction = static_cast<EdgeDirection>(id["Direction"].Int())},
          .FromNodeId = Word(row["FromNodeId"]),
          .ToNodeId = Word(row["ToNodeId"]),
          .Modes = static_cast<uint8_t>(row["Modes"].Int()),
          .Facility = static_cast<TransportFacility>(row["Facility"].Int()),
          .Surface = static_cast<TransportSurface>(row["Surface"].Int()),
          .WidthM = row["WidthM"].Num(),
          .LaneCount = static_cast<uint8_t>(row["LaneCount"].Int()),
          .Layer = row["Layer"].Int(),
          .Bridge = row["Bridge"].Bool(),
          .Tunnel = row["Tunnel"].Bool(),
          .Access = static_cast<TransportAccess>(row["Access"].Int())};
}

[[nodiscard]] inline std::shared_ptr<const World::TransportNetworkSnapshot>
Snapshot(Json::Ref root) {
  std::vector<World::TransportNode> nodes;
  std::vector<World::TransportEdge> edges;
  std::vector<GeographicBounds> coverage;
  for (size_t at = 0; at < root["Nodes"].Size(); ++at) {
    const auto row = root["Nodes"][at];
    nodes.push_back({.SourceNodeId = Word(row["SourceNodeId"]),
                     .LatitudeDeg = row["LatitudeDeg"].Num(),
                     .LongitudeDeg = row["LongitudeDeg"].Num()});
  }
  for (size_t at = 0; at < root["Edges"].Size(); ++at) { edges.push_back(Edge(root["Edges"][at])); }
  for (size_t at = 0; at < root["Coverage"].Size(); ++at) {
    const auto row = root["Coverage"][at];
    coverage.push_back({.WestDeg = row["WestDeg"].Num(),
                        .SouthDeg = row["SouthDeg"].Num(),
                        .EastDeg = row["EastDeg"].Num(),
                        .NorthDeg = row["NorthDeg"].Num()});
  }
  const auto identity = root["SourceIdentity"];
  return std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{
          .Graph = World::TransportTopology(
              {.DatasetId = identity["DatasetId"].Str(), .Revision = identity["Revision"].Str()},
              std::move(nodes),
              std::move(edges),
              static_cast<size_t>(Word(root["UnclassifiedPathCount"]))),
          .Routes = {}},
      std::move(coverage),
      World::TransportLoadMetrics{});
}

[[nodiscard]] inline bool Qualifies(Json::Ref value, const char *text) {
  switch (value.GetKind()) {
    case Json::Kind::Invalid:
    case Json::Kind::Null: return true;
    case Json::Kind::Bool: return !value.Bool();
    case Json::Kind::Number: return value.Num() == 0;
    case Json::Kind::String: return value.StrEquals(text);
    default: return false;
  }
}

[[nodiscard]] inline std::array<uint8_t, 32> Digest(Json::Ref row) {
  std::array<uint8_t, 32> result{};
  CHECK(row.Size() == result.size(), "captured feature bindings retain their exact digest width");
  for (size_t at = 0; at < result.size(); ++at) {
    result[at] = static_cast<uint8_t>(row[at].Int());
  }
  return result;
}

using Omission = std::pair<uint64_t, std::array<uint8_t, 32>>;

[[nodiscard]] inline std::set<Omission> Omissions(Json::Ref root) {
  std::set<Omission> omissions;
  for (size_t at = 0; at < root["Omissions"].Size(); ++at) {
    const auto row = root["Omissions"][at];
    constexpr int kUnqualifiedLevel = 4;
    constexpr int kMissingTopology = 5;
    if (row["Reason"].Int() == kUnqualifiedLevel || row["Reason"].Int() == kMissingTopology) {
      omissions.emplace(Word(row["Source"]), Digest(row["Feature"]));
    }
  }
  return omissions;
}

inline void BindRecipes(Json::Ref root, Source &result, Json::Ref recipes) {
  const auto omissions = Omissions(root);
  std::set<uint64_t> omittedPaths;
  std::map<uint64_t, size_t> components;
  for (size_t at = 0; at < root["Paths"].Size(); ++at) {
    const auto path = root["Paths"][at];
    const auto id = Word(path["PathId"]);
    const auto recipe = recipes[std::to_string(id).c_str()];
    const auto properties = recipe["source_properties"];
    if (recipe["maximum_gradient"].GetKind() == Json::Kind::Number &&
        Qualifies(properties["indoor"], "no") && Qualifies(properties["level"], "0")) {
      result.Profiles.push_back(
          {.PathId = id, .MaximumGradient = recipe["maximum_gradient"].Num()});
    }
    CHECK(path["ReferenceCount"].Int() == 1,
          "the actual source capture has one reference per path");
    const auto reference = root["References"][static_cast<size_t>(path["FirstReference"].Int())];
    const auto local = path["LocalWayBinding"];
    if (omissions.contains({Word(reference["Source"]), Digest(local["FeatureContent"])})) {
      omittedPaths.insert(id);
    }
    const auto feature = Word(reference["ProviderElementId"]);
    const auto ring = local["FeatureRingOrdinal"].Int();
    constexpr uint64_t kService = 1047395490;
    constexpr uint64_t kPath = 1604876210;
    constexpr int kServiceRing = 4;
    constexpr int kPathRing = 11;
    constexpr int kOpenRing = 6;
    if (feature == kService && ring == kServiceRing) { components[id] = 0; }
    if (feature == kPath && ring == kPathRing) { components[id] = 1; }
    if (feature == kPath && ring == kOpenRing) { components[id] = 2; }
  }
  std::set<uint64_t> unknown;
  uint64_t previousPath = 0;
  uint32_t previousSegment = 0;
  for (const auto &edge : result.Snapshot->Topology().Edges()) {
    if (omittedPaths.contains(edge.Id.PathId)) {
      unknown.insert(edge.FromNodeId);
      unknown.insert(edge.ToNodeId);
    }
    const auto found = components.find(edge.Id.PathId);
    if (found != components.end() &&
        (edge.Id.PathId != previousPath || edge.Id.SegmentOrdinal != previousSegment)) {
      result.Components[found->second].push_back(edge.Id);
    }
    previousPath = edge.Id.PathId;
    previousSegment = edge.Id.SegmentOrdinal;
  }
  result.Unknown.assign(unknown.begin(), unknown.end());
  std::ranges::sort(result.Profiles, {}, &Generators::GroundPortSourceProfile::PathId);
}

[[nodiscard]] inline std::expected<Source, std::string> Load() {
  constexpr auto kMetadataBytes = size_t{1024} * 1024;
  const auto raw = ReadCapture();
  const auto meta = ReadTextFile(std::string(kRoot) + "manifest.json", kMetadataBytes);
  const auto recipeBytes = ReadTextFile(std::string(kRoot) + "recipes.json", kMetadataBytes);
  if (!raw || !meta || !recipeBytes) {
    return std::unexpected("original source inputs unavailable");
  }
  Json manifest;
  Json native;
  Json recipes;
  if (!manifest.Parse(meta->data(), meta->size()) || !native.Parse(raw->data(), raw->size()) ||
      !recipes.Parse(recipeBytes->data(), recipeBytes->size())) {
    return std::unexpected("captured input JSON is invalid");
  }
  const auto m = manifest.Root();
  if (raw->size() != Word(m["native_raw_bytes"]) ||
      Sha256Hex(*raw) != m["native_raw_sha256"].Str() ||
      Sha256Hex(*recipeBytes) != m["files"]["recipes.json"]["sha256"].Str() ||
      !native.Root()["schema"].StrEquals("outshine/native-mvt-snapshot-dump/1")) {
    return std::unexpected("pinned source inputs changed");
  }
  Source result{
      .Snapshot = Snapshot(native.Root()), .Profiles = {}, .Unknown = {}, .Components = {}};
  BindRecipes(native.Root(), result, recipes.Root());
  return result;
}
}
#endif
