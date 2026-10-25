#include "building_selection_observer.h"
#include "building_surface_inputs.h"
#include "building_view_geometry.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "StructureArtifact.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using Json = nlohmann::json;
constexpr size_t kInputBytesMost = size_t{64} * 1024 * 1024;
constexpr size_t kResidentBytesMost = size_t{128} * 1024 * 1024;

std::vector<uint8_t> Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  const auto size = stream.tellg();
  if (!stream || size <= 0 ||
      std::cmp_greater(static_cast<std::streamoff>(size), kInputBytesMost)) {
    return {};
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char *>(bytes.data()), size);
  return stream ? bytes : std::vector<uint8_t>{};
}

bool LoadForms(PreparedStructureTile &base, const Json &input, const std::filesystem::path &root) {
  if (base.Surfaces.size() != base.Structures.size()) { return false; }
  std::vector<bool> assigned(base.Structures.size(), false);
  for (const auto &form : input.at("forms")) {
    const uint32_t cell = form.at("cell");
    auto surfaces = DecodeBuildingSurfaceBlock(Read(root / form.at("file").get<std::string>()),
                                               kResidentBytesMost);
    if (!surfaces) { return false; }
    size_t next = 0;
    for (size_t index = 0; index < base.Structures.size(); ++index) {
      if (base.Structures[index].Layout.Cell.Index != cell) { continue; }
      if (assigned[index] || next >= surfaces->size() ||
          !CompatiblePreparedSurface(base.Surfaces[index], (*surfaces)[next])) {
        return false;
      }
      base.Surfaces[index] = std::move((*surfaces)[next++]);
      assigned[index] = true;
    }
    if (next != surfaces->size()) { return false; }
  }
  return std::ranges::all_of(assigned, [](bool value) { return value; });
}

Json Sources(const PreparedStructureTile &base,
             const RawTile &view,
             bool surfaces,
             bool fullView = false) {
  const std::optional<Vec3> eye =
      view.EyeEcef ? std::optional<Vec3>(*view.EyeEcef - base.AnchorEcef) : std::nullopt;
  const BuildingMesh native;
  auto scratch = native.Scratch();
  auto sources = Json::array();
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto &source = base.Structures[index];
    if (fullView && source.Layout.Cell.Index != view.RequestedCell) { continue; }
    auto plan = PreparedStructurePlan(
        source,
        base.PointsLatLon,
        base.Holes,
        std::span(base.CornerAslM).subspan(source.CornerFirst, source.Layout.PointCount),
        base.AnchorEcef);
    plan.Prepared = &base.Surfaces[index];
    const auto bounds = native.SourceEnvelopeBounds(plan, *scratch);
    const auto shell = native.ShellSurfaceErrorM(plan, *scratch);
    Json row{{"index", index},
             {"cell", source.Layout.Cell.Index},
             {"id", source.Layout.SourceId.Id},
             {"kind", source.Layout.SourceId.Kind},
             {"shell_error_m", shell ? Json(*shell) : Json(nullptr)},
             {"bounds", nullptr}};
    if (bounds) {
      row["bounds"] = {{"min", {bounds->Min[0], bounds->Min[1], bounds->Min[2]}},
                       {"max", {bounds->Max[0], bounds->Max[1], bounds->Max[2]}}};
    }
    if (surfaces) {
      row["surface"] = Test::InputSurface(base.Surfaces[index], source.Layout.MinimumHeightM, eye);
    }
    if (fullView) {
      row["footprint_lat_lon"] = plan.RingLatLon;
      row["corner_asl_m"] = plan.CornerAslM;
      row["height_m"] = plan.HeightM;
      row["minimum_height_m"] = plan.MinimumHeightM;
      row["base_asl_m"] = plan.BaseAslM;
      row["seat_asl_m"] = plan.SeatAslM;
      row["foot_asl_m"] = plan.FootAslM;
    }
    sources.push_back(std::move(row));
  }
  return sources;
}

Json Bake(const PreparedStructureTile &base,
          RawTile view,
          size_t captureTriangleLimit = 0,
          const BakedTile *fullView = nullptr,
          Test::BuildingSelectionMode mode = Test::BuildingSelectionMode::Projected) {
  Test::BuildingSelectionObserver observer(
      base,
      captureTriangleLimit,
      fullView != nullptr ? Test::BuildingSelectionMode::FullView : mode);
  auto scratch = observer.Scratch();
  const auto start = std::chrono::steady_clock::now();
  auto baked = BakePreparedStructures(base, view, observer, *scratch);
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  Json row{{"cell", view.RequestedCell ? Json(*view.RequestedCell) : Json(nullptr)},
           {"elapsed_s", elapsed},
           {"accepted", baked.has_value()},
           {"mesh_calls", observer.TakeMeshes()},
           {"projection_calls", observer.TakeProjections()}};
  if (captureTriangleLimit != 0) {
    row["capture_complete"] = observer.CaptureComplete();
    row["captured_projection_calls"] = observer.CapturedCalls();
  }
  if (!baked) {
    row["error"] = Describe(baked.error());
    return row;
  }
  if (fullView != nullptr) {
    const bool same = Test::ViewGeometryEqual(baked->Built, fullView->Built);
    const bool metadata = baked->Prints == fullView->Prints &&
                          baked->FootprintDetails == fullView->FootprintDetails &&
                          baked->UnsupportedMeshes == fullView->UnsupportedMeshes &&
                          baked->SkippedRings == fullView->SkippedRings;
    row["archived_geometry_identical"] = same;
    row["archived_metadata_identical"] = metadata;
    row["accepted"] = same && metadata;
  }
  if (fullView != nullptr || mode == Test::BuildingSelectionMode::FullView) {
    row["geometry"] =
        Test::ViewGeometryInputs(fullView != nullptr ? fullView->Built : baked->Built);
  }
  row["details"] = Json::array();
  for (const auto detail : baked->FootprintDetails) {
    row["details"].push_back(static_cast<unsigned>(detail));
  }
  row["footprints"] = baked->Prints.size();
  row["unsupported"] = baked->UnsupportedMeshes;
  row["blocks"] = baked->Blocks;
  row["lumped"] = baked->Lumped;
  row["wall_vertices"] = baked->Built.WallCorners.size();
  row["roof_vertices"] = baked->Built.RoofCorners.size();
  row["fallback_triangles"] = (baked->Built.WallRun.size() + baked->Built.RoofRun.size()) / 3;
  row["cluster_index_references"] = baked->Walls.Index.size() + baked->Roofs.Index.size();
  row["built_bytes"] = baked->Built.UsedBytes();
  row["heap_bytes"] = baked->HeapBytes();
  return row;
}

enum class ExportMode { Selection, Surfaces, ArchivedView, CellView, Captures };

bool ValidInput(const Json &input) {
  if (!input.is_object()) { return false; }
  for (const auto *field : {"parent", "index", "view_key", "view_file"}) {
    if (!input.contains(field) || !input[field].is_string()) { return false; }
  }
  if (!input.contains("cluster_triangles") || !input["cluster_triangles"].is_number_unsigned() ||
      input["cluster_triangles"].get<uint64_t>() > std::numeric_limits<uint32_t>::max() ||
      !input.contains("forms") || !input["forms"].is_array()) {
    return false;
  }
  return std::ranges::all_of(input["forms"], [](const auto &form) {
    return form.is_object() && form.contains("cell") && form["cell"].is_number_unsigned() &&
           form["cell"].template get<uint64_t>() <= std::numeric_limits<uint32_t>::max() &&
           form.contains("file") && form["file"].is_string();
  });
}

struct Paths {
  std::filesystem::path Root;
  std::filesystem::path Target;
  ExportMode Mode = ExportMode::Selection;
  std::optional<uint32_t> Cell = std::nullopt;
  size_t CaptureTriangleLimit = 0;
};

bool HasCell(const PreparedStructureTile &base, std::optional<uint32_t> cell) {
  return cell && std::ranges::any_of(base.Structures, [&](const auto &source) {
           return source.Layout.Cell.Index == cell;
         });
}

int Run(const Json &input, const Paths &paths) {
  const auto &root = paths.Root;
  auto base = DecodePreparedStructureIndex(Read(root / input.at("index").get<std::string>()),
                                           kResidentBytesMost);
  auto artifact = DecodeStructureArtifact(Read(root / input.at("view_file").get<std::string>()),
                                          input.at("view_key").get<std::string>(),
                                          0);
  if (!base || !artifact || !artifact->SelectedView || artifact->RequestedDetail ||
      !LoadForms(*base, input, root)) {
    return 2;
  }
  const auto &selected = *artifact->SelectedView;
  RawTile view;
  view.Eye = selected.Eye;
  view.EyeEcef = selected.EyeEcef;
  view.Projection = selected.Projection;
  view.ClusterTriangles = input.at("cluster_triangles").get<uint32_t>();
  const bool fullView =
      paths.Mode == ExportMode::ArchivedView || paths.Mode == ExportMode::CellView;
  if (fullView) {
    view.RequestedCell = paths.Mode == ExportMode::CellView ? paths.Cell : artifact->RequestedCell;
    if (!HasCell(*base, view.RequestedCell)) { return 2; }
  }
  Json report{
      {"parent", input.at("parent")},
      {"anchor_ecef", {base->AnchorEcef[0], base->AnchorEcef[1], base->AnchorEcef[2]}},
      {"sources", Sources(*base, view, paths.Mode == ExportMode::Surfaces || fullView, fullView)},
      {"results", Json::array()},
      {"scope",
       "Actual unchanged native producer; observer costs included; no GPU or world admission."}};
  report["view"] = {
      {"longitude", selected.Eye.LongitudeDeg},
      {"latitude", selected.Eye.LatitudeDeg},
      {"eye_ecef",
       selected.EyeEcef
           ? Json{(*selected.EyeEcef)[0], (*selected.EyeEcef)[1], (*selected.EyeEcef)[2]}
           : Json(nullptr)},
      {"focal_px", selected.Projection.FocalPx},
      {"error_px", selected.Projection.AllowedErrorPx},
      {"cluster_triangles", view.ClusterTriangles}};
  bool accepted = true;
  if (paths.Mode == ExportMode::ArchivedView) {
    if (!view.RequestedCell) { return 2; }
    report["view_key"] = input.at("view_key");
    report["view"]["cell"] = *view.RequestedCell;
    report["view"]["reuse_radius_m"] = selected.EyeRadiusM;
    report["results"].push_back(Bake(*base, view, 0, &*artifact));
    accepted = report["results"].back().at("accepted");
    report["scope"] = "Original complete 3D cell; native full-hierarchy replay byte-checked "
                      "against the archived geometry. Opacity/material binding unqualified.";
  } else if (paths.Mode == ExportMode::CellView) {
    if (!view.RequestedCell) { return 2; }
    report["view_key"] = input.at("view_key");
    report["view"]["cell"] = *view.RequestedCell;
    report["view"]["reuse_radius_m"] = selected.EyeRadiusM;
    report["results"].push_back(
        Bake(*base, view, 0, nullptr, Test::BuildingSelectionMode::FullView));
    accepted = report["results"].back().at("accepted");
    report["scope"] = "Native unprojected cell control from original inputs and camera. "
                      "No archived-geometry identity claim; failed sources remain explicit.";
  } else if (paths.Mode == ExportMode::Captures) {
    view.RequestedCell = artifact->RequestedCell;
    report["results"].push_back(Bake(*base, view, paths.CaptureTriangleLimit));
    const auto &row = report["results"].back();
    accepted = row.at("accepted").get<bool>() && row.at("capture_complete").get<bool>() &&
               row.at("captured_projection_calls").get<size_t>() != 0;
    report["scope"] = "Actual archived cell/view; full input surfaces and exact native capture "
                      "emissions. Opacity/culling unqualified.";
  } else if (paths.Mode == ExportMode::Selection) {
    report["results"].push_back(Bake(*base, view));
    accepted = report["results"].back().at("accepted");
    for (const auto &form : input.at("forms")) {
      view.RequestedCell = form.at("cell").get<uint32_t>();
      report["results"].push_back(Bake(*base, view));
      accepted = accepted && report["results"].back().at("accepted").get<bool>();
      std::cerr << "cell " << *view.RequestedCell << '\n';
    }
  } else {
    report["scope"] = "Native analytic surfaces; no Bake/Project/Mesh calls or opacity guarantee.";
  }
  std::ofstream stream(paths.Target);
  stream << report.dump(2) << '\n';
  return stream && accepted ? 0 : 3;
}

template <typename Number> std::optional<Number> PositiveNumber(std::string_view text) {
  Number number = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number == 0) {
    return std::nullopt;
  }
  return number;
}

std::optional<Paths> ParseOptions(int argc, char *const *argv) {
  if (argc < 3 || argc > 5) { return std::nullopt; }
  Paths paths{.Root = std::filesystem::path(argv[1]).parent_path(), .Target = argv[2]};
  if (argc == 3) { return paths; }
  const std::string_view mode(argv[3]);
  if (argc == 4) {
    if (mode == "--surfaces") {
      paths.Mode = ExportMode::Surfaces;
    } else if (mode == "--full-view") {
      paths.Mode = ExportMode::ArchivedView;
    } else {
      return std::nullopt;
    }
    return paths;
  }
  if (mode == "--full-cell") {
    paths.Cell = PositiveNumber<uint32_t>(argv[4]);
    if (!paths.Cell) { return std::nullopt; }
    paths.Mode = ExportMode::CellView;
  } else if (mode == "--captures") {
    const auto limit = PositiveNumber<size_t>(argv[4]);
    if (!limit) { return std::nullopt; }
    paths.CaptureTriangleLimit = *limit;
    paths.Mode = ExportMode::Captures;
  } else {
    return std::nullopt;
  }
  return paths;
}
}

int main(int argc, char *const *argv) {
  const auto paths = ParseOptions(argc, argv);
  if (!paths || std::filesystem::exists(paths->Target)) { return 2; }
  std::ifstream stream(argv[1]);
  const auto input = nlohmann::json::parse(stream, nullptr, false);
  if (input.is_discarded() || !ValidInput(input)) { return 2; }
  return Run(input, *paths);
}
