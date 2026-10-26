#include "MeshPairs.h"
#include "DepthEnvelope.h"
#include "fixed_coverage/UncertainPatch.h"
#include "vertex_projection/Envelope.h"
#include "vertex_projection/Input.h"
#include "fixed_coverage/FixedPatch.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ios>
#include <map>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace P = PatchSelection;
namespace F = FixedCoverage;
using Json = nlohmann::json;

namespace {
constexpr uint32_t Side = P::DepthEnvelope::Side;
constexpr size_t Wanted = 128;
constexpr size_t MaximumElements = 1000000;
using Matrix = std::array<float, 16>;
using Clip = std::array<float, 4>;
using Point2 = std::array<double, 2>;

struct Input {
  P::Mesh Geometry;
  std::vector<Clip> ClipPoints;
  Projection::Input Transform;
  uint32_t Id = 0;
};

struct Candidate {
  F::SourceQuad Quad;
  std::array<P::Position, 4> Local;
  std::array<uint32_t, 2> Faces;
  uint32_t Mesh = 0;
  size_t InputSlot = 0;
  double Area = 0;
};

struct Ranked {
  std::vector<Candidate> Items;
  Json Work;
  uint64_t Faces = 0;
  uint64_t Pairs = 0;
  uint64_t Comparisons = 0;
  double Seconds = 0;
};

enum class ProjectionPolicy { ExactInput, RationalUncertainty, IntegerUncertainty };

struct UncertaintyWork {
  uint64_t Calls = 0;
  Projection::EnvelopeCosts Known;
};

struct PreparedCandidate {
  F::FixedPatch Packet;
  Json Source;
};

struct Evaluated {
  Json Case;
  Json ReducedCase;
  Json Report;
};

template <size_t N> std::expected<std::array<float, N>, std::string> Floats(const Json &array) {
  if (!array.is_array() || array.size() != N) { return std::unexpected("invalid float array"); }
  std::array<float, N> result{};
  for (size_t i = 0; i < N; ++i) {
    if (!array[i].is_number()) { return std::unexpected("non-numeric float coordinate"); }
    const auto value = array[i].get<double>();
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
      return std::unexpected("coordinate outside finite float32");
    }
    result[i] = static_cast<float>(value);
  }
  return result;
}

std::expected<uint32_t, std::string> Index(const Json &value) {
  if (!value.is_number_integer() || value < 0 || value > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("invalid unsigned index");
  }
  return value.get<uint32_t>();
}

std::expected<P::Mesh, std::string> Mesh(const Json &source) {
  if (!source.is_object() || !source.contains("positions_f32") || !source.contains("faces") ||
      !source["positions_f32"].is_array() || !source["faces"].is_array() ||
      source["positions_f32"].size() > MaximumElements ||
      source["faces"].size() > MaximumElements) {
    return std::unexpected("invalid mesh fields");
  }
  P::Mesh mesh;
  std::map<std::array<uint32_t, 3>, uint32_t> canonical;
  std::vector<uint32_t> remapped;
  for (const auto &value : source["positions_f32"]) {
    auto point = Floats<3>(value);
    if (!point) { return std::unexpected(point.error()); }
    const std::array key{std::bit_cast<uint32_t>((*point)[0]),
                         std::bit_cast<uint32_t>((*point)[1]),
                         std::bit_cast<uint32_t>((*point)[2])};
    auto [at, fresh] = canonical.try_emplace(key, static_cast<uint32_t>(mesh.Positions.size()));
    if (fresh) { mesh.Positions.push_back(*point); }
    remapped.push_back(at->second);
  }
  for (const auto &value : source["faces"]) {
    if (!value.is_array() || value.size() != 3) { return std::unexpected("invalid triangle"); }
    P::Face face{};
    for (size_t i = 0; i < 3; ++i) {
      const auto index = Index(value[i]);
      if (!index || *index >= remapped.size()) {
        return std::unexpected("index outside positions");
      }
      face[i] = remapped[*index];
    }
    mesh.Faces.push_back(face);
  }
  return mesh;
}

Clip Multiply(const Matrix &matrix, const Clip &point) {
  Clip result{};
  for (size_t row = 0; row < 4; ++row) {
    result[row] = matrix[row] * point[0] + matrix[4 + row] * point[1] + matrix[8 + row] * point[2] +
                  matrix[12 + row] * point[3];
  }
  return result;
}

std::expected<Input, std::string>
ReadMesh(const Json &source, const Matrix &mvp, const std::array<float, 3> &shift) {
  if (!source.contains("id") || !source.contains("model_f32")) {
    return std::unexpected("missing source identity/placement");
  }
  const auto id = Index(source["id"]);
  auto model = Floats<16>(source["model_f32"]);
  auto mesh = Mesh(source);
  if (!id || !model || !mesh) { return std::unexpected("invalid source identity/placement/mesh"); }
  Projection::Input transform{.Model = *model, .View = mvp};
  std::ranges::copy(shift, transform.Shift.begin());
  for (size_t i = 0; i < 3; ++i) { (*model)[12 + i] += shift[i]; }
  Input result{.Geometry = std::move(*mesh), .ClipPoints = {}, .Transform = transform, .Id = *id};
  for (const auto &point : result.Geometry.Positions) {
    result.ClipPoints.push_back(Multiply(mvp, Multiply(*model, {point[0], point[1], point[2], 1})));
  }
  return result;
}

std::expected<std::vector<Input>, std::string> ReadInput(const std::filesystem::path &path) {
  constexpr std::streamoff ByteLimit = std::streamoff{16} * 1024 * 1024;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() < 0 || file.tellg() > ByteLimit) {
    return std::unexpected("invalid input size");
  }
  file.seekg(0);
  const auto source = Json::parse(file, nullptr, false);
  if (source.is_discarded() || !source.is_object() ||
      !source.contains("qualified_for_page_suppression") ||
      source["qualified_for_page_suppression"] != false || !source.contains("mvp_f32") ||
      !source.contains("shift_f32") || !source.contains("meshes") || !source["meshes"].is_array() ||
      source["meshes"].size() > MaximumElements) {
    return std::unexpected("invalid calibration input");
  }
  auto mvp = Floats<16>(source["mvp_f32"]);
  auto shift = Floats<3>(source["shift_f32"]);
  if (!mvp || !shift) { return std::unexpected("invalid camera transform"); }
  std::vector<Input> inputs;
  for (const auto &part : source["meshes"]) {
    auto mesh = ReadMesh(part, *mvp, *shift);
    if (!mesh) { return std::unexpected(mesh.error()); }
    inputs.push_back(std::move(*mesh));
  }
  return inputs;
}

std::vector<Point2> ClipAxis(const std::vector<Point2> &polygon, size_t axis, bool upper) {
  if (polygon.empty()) { return {}; }
  const double bound = upper ? 1 : -1;
  const auto distance = [&](const Point2 &p) { return upper ? bound - p[axis] : p[axis] - bound; };
  std::vector<Point2> clipped;
  auto previous = polygon.back();
  double before = distance(previous);
  for (const auto &point : polygon) {
    const double now = distance(point);
    if ((now >= 0) != (before >= 0)) {
      const double part = before / (before - now);
      clipped.push_back({previous[0] + (point[0] - previous[0]) * part,
                         previous[1] + (point[1] - previous[1]) * part});
    }
    if (now >= 0) { clipped.push_back(point); }
    previous = point;
    before = now;
  }
  return clipped;
}

double ApproximateArea(const F::SourceQuad &quad) {
  std::vector<Point2> polygon;
  for (const auto &point : quad.Points) {
    if (!(point[3] > 0) || !std::isfinite(point[3])) { return 0; }
    polygon.push_back(
        {static_cast<double>(point[0]) / point[3], static_cast<double>(point[1]) / point[3]});
  }
  for (size_t axis = 0; axis < 2; ++axis) {
    polygon = ClipAxis(polygon, axis, false);
    polygon = ClipAxis(polygon, axis, true);
  }
  double area = 0;
  for (size_t i = 0; i < polygon.size(); ++i) {
    const auto &a = polygon[i];
    const auto &b = polygon[(i + 1) % polygon.size()];
    area += a[0] * b[1] - a[1] * b[0];
  }
  return std::isfinite(area) ? std::abs(area) * .5 : 0;
}

Json Costs(const P::Costs &work) {
  return {{"edge_records", work.Edges},
          {"sort_comparisons", work.SortComparisons},
          {"neighbor_candidates", work.NeighborCandidates},
          {"unpairable_edge_groups", work.UnpairableEdgeGroups},
          {"scratch_capacity_bytes_upper_sum", work.ScratchCapacityBytes},
          {"result_capacity_bytes", work.ResultCapacityBytes},
          {"explicit_pairing_allocation_requests", work.AllocationRequests}};
}

bool Cyclic(const P::Face &face, const P::Face &expected) {
  for (size_t shift = 0; shift < 3; ++shift) {
    if (face[shift] == expected[0] && face[(shift + 1) % 3] == expected[1] &&
        face[(shift + 2) % 3] == expected[2]) {
      return true;
    }
  }
  return false;
}

bool SourcePairsPreserved(const P::Mesh &mesh, const P::Selection &selection) {
  std::vector<uint8_t> used(mesh.Faces.size());
  for (const auto &pair : selection.Pairs) {
    for (const auto id : pair.Faces) {
      if (id >= mesh.Faces.size() || used[id] != 0) { return false; }
      used[id] = 1;
    }
    const auto &c = pair.Corners;
    if (!Cyclic(mesh.Faces[pair.Faces[0]], {c[0], c[1], c[3]}) ||
        !Cyclic(mesh.Faces[pair.Faces[1]], {c[2], c[3], c[1]})) {
      return false;
    }
  }
  return true;
}

void AddCandidates(const Input &input,
                   size_t inputSlot,
                   const P::Selection &selection,
                   std::vector<Candidate> &candidates) {
  for (const auto &pair : selection.Pairs) {
    Candidate candidate{.Quad = {.Points = {}, .Opaque = true, .Complete = true},
                        .Local = {},
                        .Faces = pair.Faces,
                        .Mesh = input.Id,
                        .InputSlot = inputSlot};
    for (size_t i = 0; i < 4; ++i) {
      candidate.Quad.Points[i] = input.ClipPoints[pair.Corners[i]];
      candidate.Local[i] = input.Geometry.Positions[pair.Corners[i]];
    }
    candidate.Area = ApproximateArea(candidate.Quad);
    if (candidate.Area > 0) { candidates.push_back(candidate); }
  }
}

std::expected<Ranked, std::string> Rank(std::span<const Input> inputs, P::Policy policy) {
  const auto began = std::chrono::steady_clock::now();
  std::vector<Candidate> candidates;
  Json costs = Json::array();
  uint64_t faces = 0;
  uint64_t pairs = 0;
  for (size_t inputSlot = 0; inputSlot < inputs.size(); ++inputSlot) {
    const auto &input = inputs[inputSlot];
    const auto selected = P::Select(input.Geometry, policy);
    if (!selected) { return std::unexpected(std::string(selected.error())); }
    if (!SourcePairsPreserved(input.Geometry, *selected)) {
      return std::unexpected("pair changed source topology or reused a face");
    }
    auto work = Costs(selected->Work);
    work["mesh"] = input.Id;
    costs.push_back(std::move(work));
    faces += input.Geometry.Faces.size();
    pairs += selected->Pairs.size();
    AddCandidates(input, inputSlot, *selected, candidates);
  }
  uint64_t comparisons = 0;
  std::ranges::stable_sort(candidates, [&](const Candidate &a, const Candidate &b) {
    ++comparisons;
    return a.Area > b.Area;
  });
  return Ranked{
      .Items = std::move(candidates),
      .Work = std::move(costs),
      .Faces = faces,
      .Pairs = pairs,
      .Comparisons = comparisons,
      .Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count()};
}

std::expected<PreparedCandidate, std::string> ProjectCandidate(const Candidate &candidate,
                                                               const Input &input,
                                                               ProjectionPolicy projection,
                                                               UncertaintyWork &work) {
  Json source{{"points", candidate.Quad.Points},
              {"mesh", candidate.Mesh},
              {"triangles", candidate.Faces},
              {"local_vertices_f32", candidate.Local}};
  std::expected<F::Projected, F::Unknown> projected = std::unexpected(F::Unknown::Incomplete);
  if (projection != ProjectionPolicy::ExactInput) {
    F::UncertainQuad quad{.FrontWindowOrientation = -1, .Opaque = true, .Complete = true};
    source["clip_bounds"] = Json::array();
    source["front_sign"] = quad.FrontWindowOrientation;
    for (size_t i = 0; i < 4; ++i) {
      auto transform = input.Transform;
      transform.Point = {candidate.Local[i][0], candidate.Local[i][1], candidate.Local[i][2], 1};
      ++work.Calls;
      const auto hull = Projection::Enclose(transform);
      if (!hull) {
        return std::unexpected("projection-domain-" +
                               std::to_string(static_cast<int>(hull.error())));
      }
      work.Known.Inputs += hull->Work.Inputs;
      work.Known.Products += hull->Work.Products;
      work.Known.Sums += hull->Work.Sums;
      work.Known.OutwardSteps += hull->Work.OutwardSteps;
      work.Known.Divisions += hull->Work.Divisions;
      quad.Clip[i] = hull->Clip;
      Json corner = Json::array();
      for (const auto &range : hull->Clip) { corner.push_back({range.Lower, range.Upper}); }
      source["clip_bounds"].push_back(std::move(corner));
    }
    if (projection == ProjectionPolicy::IntegerUncertainty) {
      auto packet = F::PrepareInteger(quad, Side, Side);
      if (!packet) {
        return std::unexpected("integer-geometry-" +
                               std::to_string(static_cast<int>(packet.error())));
      }
      return PreparedCandidate{.Packet = *packet, .Source = std::move(source)};
    }
    projected = F::Project(quad, Side, Side);
  } else {
    projected = F::Project(candidate.Quad, Side, Side);
  }
  if (!projected) {
    return std::unexpected("geometry-" + std::to_string(static_cast<int>(projected.error())));
  }
  if (projected->SourceWindowOrientation >= 0) { return std::unexpected("backfacing"); }
  const auto packet = F::Prepare(*projected, Side, Side);
  if (!packet) {
    return std::unexpected("quantization-" + std::to_string(static_cast<int>(packet.error())));
  }
  return PreparedCandidate{.Packet = *packet, .Source = std::move(source)};
}

std::expected<Evaluated, std::string>
Evaluate(std::span<const Input> inputs,
         const Ranked &ranked,
         const std::string &name,
         ProjectionPolicy projection = ProjectionPolicy::ExactInput) {
  const auto began = std::chrono::steady_clock::now();
  const auto &candidates = ranked.Items;
  UncertaintyWork work;
  std::map<std::string, uint64_t> unknownReasons;
  Json sources = Json::array();
  Json reduced = Json::array();
  P::DepthEnvelope envelope;
  double reductionSeconds = 0;
  uint64_t unknown = 0;
  uint64_t backfacing = 0;
  for (const auto &candidate : candidates) {
    auto projected = ProjectCandidate(candidate, inputs[candidate.InputSlot], projection, work);
    if (!projected) {
      if (projected.error() == "backfacing") {
        ++backfacing;
      } else {
        ++unknown;
        ++unknownReasons[projected.error()];
      }
      continue;
    }
    const auto &prepared = projected->Packet;
    sources.push_back(std::move(projected->Source));
    const auto reductionStarted = std::chrono::steady_clock::now();
    const auto keep = envelope.Keep(prepared);
    reductionSeconds +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - reductionStarted).count();
    if (!keep) { return std::unexpected(std::string(keep.error())); }
    if (*keep) { reduced.push_back(sources.back()); }
    if (sources.size() == Wanted) { break; }
  }
  const double seconds =
      ranked.Seconds +
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  return Evaluated{
      .Case = {{"name", name},
               {"sources", sources},
               {"integer_certificate", projection == ProjectionPolicy::IntegerUncertainty}},
      .ReducedCase = {{"name", name + "-reduced"},
                      {"sources", reduced},
                      {"integer_certificate", projection == ProjectionPolicy::IntegerUncertainty}},
      .Report = {{"name", name},
                 {"faces", ranked.Faces},
                 {"paired_faces", ranked.Pairs * 2},
                 {"candidate_pairs", candidates.size()},
                 {"selected", sources.size()},
                 {"backfacing", backfacing},
                 {"unknown", unknown},
                 {"candidate_sort_comparisons", ranked.Comparisons},
                 {"candidate_storage_capacity_bytes", candidates.capacity() * sizeof(Candidate)},
                 {"seconds", seconds},
                 {"ranking_seconds", ranked.Seconds},
                 {"preparation_seconds", seconds - ranked.Seconds},
                 {"uncertainty", static_cast<int>(projection)},
                 {"unknown_reasons", unknownReasons},
                 {"envelope_calls", work.Calls},
                 {"known_envelope_work",
                  {{"inputs", work.Known.Inputs},
                   {"products", work.Known.Products},
                   {"sums", work.Known.Sums},
                   {"outward_steps", work.Known.OutwardSteps},
                   {"divisions", work.Known.Divisions}}},
                 {"source_pair_invariants", true},
                 {"reduction",
                  {{"retained", envelope.Costs().Retained},
                   {"empty", envelope.Costs().Empty},
                   {"dominated", envelope.Costs().Dominated},
                   {"cell_tests", envelope.Costs().CellTests},
                   {"covered_cell_visits", envelope.Costs().CoveredCellVisits},
                   {"depth_updates", envelope.Costs().DepthUpdates},
                   {"state_bytes", P::DepthEnvelope::StateBytes},
                   {"state_heap_allocations", 0},
                   {"seconds", reductionSeconds}}},
                 {"per_mesh_work", ranked.Work}}};
}

bool SelfChecks() {
  P::Mesh mesh{.Positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}},
               .Faces = {{0, 1, 2}, {0, 2, 3}, {1, 0, 4}}};
  const auto first = P::Select(mesh, P::Policy::FirstEdge);
  const auto aligned = P::Select(mesh, P::Policy::AlignedNormals);
  if (!first || !aligned || first->Pairs.size() != 1 || aligned->Pairs.size() != 1 ||
      first->Pairs[0].Faces != std::array<uint32_t, 2>{0, 2} ||
      aligned->Pairs[0].Faces != std::array<uint32_t, 2>{0, 1}) {
    return false;
  }
  mesh.Faces = {{0, 1, 2}, {1, 0, 3}, {1, 0, 4}};
  const auto nonmanifold = P::Select(mesh, P::Policy::AlignedNormals);
  if (!nonmanifold || !nonmanifold->Pairs.empty()) { return false; }
  mesh.Faces = {{0, 1, 2}, {0, 1, 3}};
  const auto wrongWinding = P::Select(mesh, P::Policy::AlignedNormals);
  if (!wrongWinding || !wrongWinding->Pairs.empty()) { return false; }
  mesh.Faces = {{0, 1, 5}};
  if (P::Select(mesh, P::Policy::AlignedNormals)) { return false; }
  mesh.Faces.clear();
  const auto empty = P::Select(mesh, P::Policy::AlignedNormals);
  if (!empty || !empty->Pairs.empty() || empty->Work.AllocationRequests != 0) { return false; }
  mesh.Positions[0][0] = std::numeric_limits<float>::quiet_NaN();
  return !P::Select(mesh, P::Policy::AlignedNormals);
}

bool EnvelopeChecks() {
  constexpr int32_t low = 8 * F::PreferredScale;
  constexpr int32_t high = (Side - 8) * F::PreferredScale;
  constexpr float farDepth = .25f;
  constexpr float nearDepth = .75f;
  F::FixedPatch patch;
  patch.Corners[0] = {low, low, 0, 0};
  patch.Corners[1] = {high, low, 0, 0};
  patch.Corners[2] = {high, high, 0, 0};
  patch.Corners[3] = {low, high, 0, 0};
  patch.Meta = {4, Side, Side, F::PreferredScale};
  patch.Depth[0] = farDepth;
  P::DepthEnvelope envelope;
  const auto original = envelope.Keep(patch);
  const auto repeated = envelope.Keep(patch);
  patch.Depth[0] = nearDepth;
  const auto nearer = envelope.Keep(patch);
  patch.Depth[0] = farDepth;
  const auto farther = envelope.Keep(patch);
  patch.Corners[0] = {1, 1, 0, 0};
  patch.Corners[1] = {2, 1, 0, 0};
  patch.Corners[2] = {2, 2, 0, 0};
  patch.Corners[3] = {1, 2, 0, 0};
  const auto empty = envelope.Keep(patch);
  patch.Meta[3] = 0;
  const bool invalidProfileRejected = !envelope.Keep(patch);
  patch.Meta[3] = F::PreferredScale;
  patch.Depth[0] = std::numeric_limits<float>::quiet_NaN();
  const bool invalidDepthRejected = !envelope.Keep(patch);
  return original && *original && repeated && !*repeated && nearer && *nearer && farther &&
         !*farther && empty && !*empty && invalidProfileRejected && invalidDepthRejected &&
         envelope.Costs().Retained == 2 && envelope.Costs().Dominated == 2 &&
         envelope.Costs().Empty == 1;
}

}

int main(int argc, char *const *argv) {
  if (argc != 4 || !SelfChecks() || !EnvelopeChecks()) { return 2; }
  const auto began = std::chrono::steady_clock::now();
  auto inputs = ReadInput(argv[1]);
  if (!inputs) {
    std::println(stderr, "{}", inputs.error());
    return 2;
  }
  const double commonSeconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  auto firstRank = Rank(*inputs, P::Policy::FirstEdge);
  auto alignedRank = Rank(*inputs, P::Policy::AlignedNormals);
  if (!firstRank || !alignedRank) { return 2; }
  auto first = Evaluate(*inputs, *firstRank, "native-first-edge");
  auto aligned = Evaluate(*inputs, *alignedRank, "native-aligned-normals");
  auto uncertain =
      Evaluate(*inputs, *alignedRank, "native-uncertain", ProjectionPolicy::RationalUncertainty);
  auto integer = Evaluate(
      *inputs, *alignedRank, "native-uncertain-integer", ProjectionPolicy::IntegerUncertainty);
  if (!first || !aligned || !uncertain || !integer) { return 2; }
  std::ofstream output(argv[2]);
  std::ofstream report(argv[3]);
  output << Json{{"qualified_for_page_suppression", false},
                 {"scope",
                  "CPU float projection reference only; no page or world-transform admission"},
                 {"cases",
                  Json::array({first->Case,
                               first->ReducedCase,
                               aligned->Case,
                               aligned->ReducedCase,
                               uncertain->Case,
                               uncertain->ReducedCase,
                               integer->Case,
                               integer->ReducedCase})}}
                .dump(2);
  report
      << Json{{"self_checks_passed", true},
              {"side_pixels", Side},
              {"mesh_count", inputs->size()},
              {"common_input_weld_projection_seconds", commonSeconds},
              {"methods",
               Json::array({first->Report, aligned->Report, uncertain->Report, integer->Report})},
              {"scope", "Pair selection work; allocations outside pairing and OS peak unmeasured"}}
             .dump(2);
  return output && report ? 0 : 1;
}
