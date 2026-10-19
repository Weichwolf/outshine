#include "building_selection_observer.h"
#include "building_surface_inputs.h"
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

Json Sources(const PreparedStructureTile &base, const RawTile &view, bool surfaces) {
  const std::optional<Vec3> eye =
      view.EyeEcef ? std::optional<Vec3>(*view.EyeEcef - base.AnchorEcef) : std::nullopt;
  const BuildingMesh native;
  auto scratch = native.Scratch();
  auto sources = Json::array();
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto &source = base.Structures[index];
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
    sources.push_back(std::move(row));
  }
  return sources;
}

Json Bake(const PreparedStructureTile &base, RawTile view, size_t captureTriangleLimit = 0) {
  Test::BuildingSelectionObserver observer(base, captureTriangleLimit);
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

struct Paths {
  std::filesystem::path Root;
  std::filesystem::path Target;
  bool SurfacesOnly = false;
  size_t CaptureTriangleLimit = 0;
};

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
  Json report{
      {"parent", input.at("parent")},
      {"anchor_ecef", {base->AnchorEcef[0], base->AnchorEcef[1], base->AnchorEcef[2]}},
      {"sources", Sources(*base, view, paths.SurfacesOnly)},
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
  if (paths.CaptureTriangleLimit != 0) {
    view.RequestedCell = artifact->RequestedCell;
    report["results"].push_back(Bake(*base, view, paths.CaptureTriangleLimit));
    const auto &row = report["results"].back();
    accepted = row.at("accepted").get<bool>() && row.at("capture_complete").get<bool>() &&
               row.at("captured_projection_calls").get<size_t>() != 0;
    report["scope"] = "Actual archived cell/view; full input surfaces and exact native capture "
                      "emissions. Opacity/culling unqualified.";
  } else if (!paths.SurfacesOnly) {
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
}

int main(int argc, char *const *argv) {
  const std::string_view mode = argc >= 4 ? argv[3] : "";
  size_t captureLimit = 0;
  if (argc == 5 && mode == "--captures") {
    const std::string_view limit(argv[4]);
    const auto parsed = std::from_chars(limit.data(), limit.data() + limit.size(), captureLimit);
    if (parsed.ec != std::errc{} || parsed.ptr != limit.data() + limit.size() ||
        captureLimit == 0) {
      return 2;
    }
  } else if (argc != 3 && (argc != 4 || mode != "--surfaces")) {
    return 2;
  }
  const std::filesystem::path inputPath(argv[1]);
  const std::filesystem::path outputPath(argv[2]);
  if (std::filesystem::exists(outputPath)) { return 2; }
  std::ifstream stream(inputPath);
  const auto input = nlohmann::json::parse(stream, nullptr, false);
  if (input.is_discarded()) { return 2; }
  return Run(input,
             {.Root = inputPath.parent_path(),
              .Target = outputPath,
              .SurfacesOnly = mode == "--surfaces",
              .CaptureTriangleLimit = captureLimit});
}
