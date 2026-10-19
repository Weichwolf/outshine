#include "building_capture_inputs.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test {
namespace {
using Json = nlohmann::json;
using namespace Generators;

template <typename T> bool SameBytes(const std::vector<T> &a, const std::vector<T> &b) {
  return std::ranges::equal(std::as_bytes(std::span(a)), std::as_bytes(std::span(b)));
}

bool SameMesh(const Raised &a, const Raised &b) {
  return SameBytes(a.WallCorners, b.WallCorners) && SameBytes(a.RoofCorners, b.RoofCorners) &&
         SameBytes(a.WallRun, b.WallRun) && SameBytes(a.RoofRun, b.RoofRun) &&
         SameBytes(a.WallColours, b.WallColours) && SameBytes(a.WallMaterialM, b.WallMaterialM);
}

struct GeometryRows {
  Json Chunks = Json::array();
  std::array<std::vector<StoredVertex>, 2> Triangles;

  [[nodiscard]] bool Matches(const Raised &mesh) const {
    const std::array vertices{&mesh.WallCorners, &mesh.RoofCorners};
    const std::array runs{&mesh.WallRun, &mesh.RoofRun};
    for (size_t stream = 0; stream < runs.size(); ++stream) {
      const auto &run = *runs[stream];
      if (run.size() != Triangles[stream].size()) { return false; }
      for (size_t at = 0; at < run.size(); ++at) {
        if (run[at] >= vertices[stream]->size()) { return false; }
        const auto a = std::as_bytes(std::span(&(*vertices[stream])[run[at]], 1));
        const auto b = std::as_bytes(std::span(&Triangles[stream][at], 1));
        if (!std::ranges::equal(a, b)) { return false; }
      }
    }
    return true;
  }
};

std::optional<Json> GeometryStream(std::span<const StoredVertex> vertices,
                                   std::span<const uint32_t> indices) {
  if (indices.size() % 3 != 0 ||
      !std::ranges::all_of(indices, [&](uint32_t index) { return index < vertices.size(); })) {
    return std::nullopt;
  }
  Json positions = Json::array();
  Json normals = Json::array();
  Json uv = Json::array();
  for (const auto &vertex : vertices) {
    positions.push_back({vertex.pos[0], vertex.pos[1], vertex.pos[2]});
    normals.push_back(vertex.normWord);
    uv.push_back({vertex.texture[0], vertex.texture[1]});
  }
  Json triangles = Json::array();
  for (size_t at = 0; at < indices.size(); at += 3) {
    triangles.push_back({indices[at], indices[at + 1], indices[at + 2]});
  }
  return Json{{"positions", std::move(positions)},
              {"normal_words", std::move(normals)},
              {"uv", std::move(uv)},
              {"triangles", std::move(triangles)}};
}

bool AppendChunk(GeometryRows &into,
                 const Raised &mesh,
                 std::optional<BuildingSurface::Face> face,
                 std::optional<size_t> faceIndex,
                 size_t &trianglesLeft) {
  const size_t triangles = mesh.WallRun.size() / 3 + mesh.RoofRun.size() / 3;
  if (triangles > trianglesLeft) { return false; }
  auto walls = GeometryStream(mesh.WallCorners, mesh.WallRun);
  auto roofs = GeometryStream(mesh.RoofCorners, mesh.RoofRun);
  if (!walls || !roofs) { return false; }
  trianglesLeft -= triangles;
  const std::array vertices{&mesh.WallCorners, &mesh.RoofCorners};
  const std::array runs{&mesh.WallRun, &mesh.RoofRun};
  for (size_t stream = 0; stream < runs.size(); ++stream) {
    for (const auto index : *runs[stream]) {
      into.Triangles[stream].push_back((*vertices[stream])[index]);
    }
  }
  into.Chunks.push_back({{"part", face ? Json(face->Part) : Json(nullptr)},
                         {"side", face ? Json(face->Side) : Json(nullptr)},
                         {"face_index", faceIndex ? Json(*faceIndex) : Json(nullptr)},
                         {"walls", std::move(*walls)},
                         {"roofs", std::move(*roofs)},
                         {"wall_colours", mesh.WallColours},
                         {"wall_material_metres", mesh.WallMaterialM}});
  return true;
}

std::expected<void, StructureMeshError> EmitSource(const StructurePlan &plan,
                                                   const BuildingSurface &source,
                                                   BuildingSurfaceEmission mode,
                                                   std::span<const BuildingSurface::Face> faces,
                                                   std::span<const BuildingSurfacePatch> patches,
                                                   BuildingScratch &scratch,
                                                   Raised &into) {
  switch (mode) {
    case BuildingSurfaceEmission::FullMesh: return BuildingMesh{}.Mesh(plan, scratch, into);
    case BuildingSurfaceEmission::VisibleFaces: return source.MeshVisible(faces, scratch, into);
    case BuildingSurfaceEmission::Patches: source.MeshPatches(patches, into); return {};
  }
  return std::unexpected(StructureMeshError::InvalidPlan);
}

struct SourceRows {
  GeometryRows Full;
  GeometryRows Emitted;
  std::vector<BuildingSurface::Face> Faces;
  std::vector<BuildingSurface::Face> AllFaces;
};

std::expected<void, std::string> SourceFaces(const BuildingSurface &source,
                                             std::span<const uint8_t> visible,
                                             BuildingSurfaceEmission mode,
                                             BuildingScratch &scratch,
                                             SourceRows &rows,
                                             size_t &trianglesLeft) {
  for (size_t at = 0; at < source.FaceCount(); ++at) {
    const auto face = source.FaceAt(at);
    rows.AllFaces.push_back(face);
    if (visible[at] != 0) { rows.Faces.push_back(face); }
    if (mode == BuildingSurfaceEmission::FullMesh) { continue; }
    Raised chunk;
    const auto made = source.MeshVisible(std::span(&face, 1), scratch, chunk);
    if (!made) { return std::unexpected(std::string(Describe(made.error()))); }
    if (!AppendChunk(rows.Full, chunk, face, at, trianglesLeft) ||
        (mode == BuildingSurfaceEmission::VisibleFaces && visible[at] != 0 &&
         !AppendChunk(rows.Emitted, chunk, face, at, trianglesLeft))) {
      return std::unexpected("native face export is invalid or exceeds the triangle limit");
    }
  }
  return {};
}

Json PatchInput(const BuildingSurfacePatch &patch) {
  Json corners = Json::array();
  for (const auto &point : patch.Corners) { corners.push_back({point[0], point[1], point[2]}); }
  return {{"part", patch.Sample.Part},
          {"side", patch.Sample.Face},
          {"gable", patch.Sample.Gable},
          {"normal", {patch.Sample.Normal[0], patch.Sample.Normal[1], patch.Sample.Normal[2]}},
          {"corners", std::move(corners)}};
}

struct NativeReferenceInputs {
  Json Surface;
  Json Fine;
};

std::expected<NativeReferenceInputs, std::string> NativeReferences(const StructurePlan &plan,
                                                                   const Raised &surface,
                                                                   BuildingScratch &scratch,
                                                                   size_t &trianglesLeft) {
  auto finePlan = plan;
  finePlan.Coarseness = LevelOfDetail::Fine;
  finePlan.RecessedOpenings = true;
  Raised fine;
  const auto built = BuildingMesh{}.Mesh(finePlan, scratch, fine);
  if (!built) {
    return std::unexpected("complete Fine reference: " + std::string(Describe(built.error())));
  }
  GeometryRows surfaceRows;
  GeometryRows fineRows;
  if (!AppendChunk(surfaceRows, surface, {}, {}, trianglesLeft) ||
      !AppendChunk(fineRows, fine, {}, {}, trianglesLeft)) {
    return std::unexpected("complete native reference controls exceed the triangle limit");
  }
  return NativeReferenceInputs{.Surface = std::move(surfaceRows.Chunks.front()),
                               .Fine = std::move(fineRows.Chunks.front())};
}

std::expected<Json, std::string> SourceInputs(const StructurePlan &plan,
                                              const BuildingSurfaceCaptureRecord &record,
                                              size_t slot,
                                              BuildingScratch &scratch,
                                              size_t &trianglesLeft,
                                              Raised &replayed) {
  std::optional<BuildingSurface> prepared;
  const BuildingSurface *source = plan.Prepared;
  if (source == nullptr) {
    auto made = BuildingSurface::Prepare(plan, scratch);
    if (!made) { return std::unexpected(std::string(Describe(made.error()))); }
    prepared.emplace(std::move(*made));
    source = &*prepared;
  }
  const auto ready = source->RequireShapes();
  if (!ready) { return std::unexpected(std::string(Describe(ready.error()))); }
  if (record.VisibleFaces[slot].size() != source->FaceCount()) {
    return std::unexpected("record face slots differ from the native source");
  }
  const auto mode = record.Emissions[slot];
  SourceRows rows;
  const auto selected =
      SourceFaces(*source, record.VisibleFaces[slot], mode, scratch, rows, trianglesLeft);
  if (!selected) { return std::unexpected(selected.error()); }
  Raised reference;
  if (mode == BuildingSurfaceEmission::FullMesh) {
    const auto made = BuildingMesh{}.Mesh(plan, scratch, reference);
    if (!made) { return std::unexpected(std::string(Describe(made.error()))); }
    if (!AppendChunk(rows.Full, reference, {}, {}, trianglesLeft) ||
        !AppendChunk(rows.Emitted, reference, {}, {}, trianglesLeft)) {
      return std::unexpected("native whole-source export exceeds the triangle limit");
    }
  } else {
    const auto made = source->MeshVisible(rows.AllFaces, scratch, reference);
    if (!made) { return std::unexpected(std::string(Describe(made.error()))); }
  }
  Json patches = Json::array();
  for (const auto &patch : record.Patches[slot]) {
    patches.push_back(PatchInput(patch));
    if (mode != BuildingSurfaceEmission::Patches) { continue; }
    Raised chunk;
    source->MeshPatches(std::span(&patch, 1), chunk);
    if (!AppendChunk(rows.Emitted,
                     chunk,
                     BuildingSurface::Face{.Part = patch.Sample.Part, .Side = patch.Sample.Face},
                     source->FaceIndex(patch.Sample),
                     trianglesLeft)) {
      return std::unexpected("native patch export exceeds the triangle limit");
    }
  }
  Raised actual;
  const auto made =
      EmitSource(plan, *source, mode, rows.Faces, record.Patches[slot], scratch, actual);
  if (!made) { return std::unexpected(std::string(Describe(made.error()))); }
  if (!rows.Full.Matches(reference) || !rows.Emitted.Matches(actual)) {
    return std::unexpected(
        "per-face or per-patch triangle ownership does not match native emission");
  }
  auto nativeReference = NativeReferences(plan, reference, scratch, trianglesLeft);
  if (!nativeReference) { return std::unexpected(nativeReference.error()); }
  const auto accumulated =
      EmitSource(plan, *source, mode, rows.Faces, record.Patches[slot], scratch, replayed);
  if (!accumulated) { return std::unexpected(std::string(Describe(accumulated.error()))); }
  return Json{{"slot", slot},
              {"emission", static_cast<unsigned>(mode)},
              {"visible_faces", record.VisibleFaces[slot]},
              {"patch_fallback", record.PatchFallbacks[slot]},
              {"patches", std::move(patches)},
              {"reference", std::move(rows.Full.Chunks)},
              {"captured", std::move(rows.Emitted.Chunks)},
              {"native_reference", std::move(nativeReference->Surface)},
              {"native_fine_reference", std::move(nativeReference->Fine)},
              {"native_triangle_ownership_verified", true}};
}
}

std::expected<nlohmann::json, std::string>
BuildingCaptureInputs(std::span<const StructurePlan> plans,
                      const Vec3 &eye,
                      ProjectedErrorBudget projection,
                      const Generators::BuildingSurfaceCaptureRecord &record,
                      const Raised &captured,
                      size_t triangleLimit) {
  if (triangleLimit == 0 || record.FaceSizePx <= 0 || record.VisibleFaces.size() != plans.size() ||
      record.Patches.size() != plans.size() || record.PatchFallbacks.size() != plans.size() ||
      record.Emissions.size() != plans.size()) {
    return std::unexpected("complete native capture record and explicit triangle limit required");
  }
  BuildingScratch scratch;
  Raised replayed;
  Json sources = Json::array();
  size_t left = triangleLimit;
  for (size_t slot = 0; slot < plans.size(); ++slot) {
    auto row = SourceInputs(plans[slot], record, slot, scratch, left, replayed);
    if (!row) { return std::unexpected("source " + std::to_string(slot) + ": " + row.error()); }
    sources.push_back(std::move(*row));
  }
  if (!SameMesh(replayed, captured)) {
    return std::unexpected(
        "record replay differs from actual native vertices, indices or materials");
  }
  GeometryRows nativeCapture;
  if (!AppendChunk(nativeCapture, captured, {}, {}, left)) {
    return std::unexpected("native capture control exceeds the triangle limit");
  }
  return Json{{"eye_offset_ecef", {eye[0], eye[1], eye[2]}},
              {"focal_px", projection.FocalPx},
              {"error_px", projection.AllowedErrorPx},
              {"cube_face_pixels", record.FaceSizePx},
              {"sources", std::move(sources)},
              {"native_capture", std::move(nativeCapture.Chunks.front())},
              {"stored_triangles", triangleLimit - left},
              {"native_mesh_replay_byte_identical", true},
              {"fine_reference_detail", static_cast<unsigned>(LevelOfDetail::Fine)},
              {"fine_reference_recessed_openings", true},
              {"opaque_for_coverage", nullptr},
              {"runtime_culling", nullptr}};
}
}
