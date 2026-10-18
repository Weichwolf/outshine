#include "SubjectLightingUniform.h"
#include "math/Vec4.h"
#include "SurfaceBindings.h"
#include "VertexUpload.h"
#include "NormalUpload.h"
#include <format>
#include "GroundLattice.h"
#include "SubjectDraw.h"
#include "BatchVisibility.h"
#include <optional>
#include "math/Vec3.h"

#include "FragmentArms.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <expected>
#include <ratio>

#include <cstdint>
#include <span>
#include <new>
#include <limits>

#include "Heap.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SceneTargets.h"
#include "VertexArms.h"
#include "SurfaceState.h"

namespace outshine::Render {
namespace Says {
constexpr auto MissingMeshAttributes = "mesh vertex layout requires an absent attribute stream";
constexpr auto InvalidMeshLayout = "mesh batch has an invalid vertex layout";
constexpr auto InvalidMeshInstances = "mesh instance range exceeds GPU addressing capacity";
constexpr auto PieceInstanceLimit = "piece instance count exceeds its declared capacity";
constexpr auto MissingPiece = "instance update names no resident piece";
}

namespace {
bool Uploaded(Native::Result<void> result, std::string &error) {
  if (result) { return true; }
  error = std::move(result.error().Message);
  return false;
}

bool UsesPrevious(const SubjectPose &pose, std::span<const DrawBatch> batches) {
  return pose.PrevVerts.Stands() && std::ranges::any_of(batches, [](const DrawBatch &batch) {
           return batch.Motion == VertexMotion::Deforming;
         });
}

}

namespace {

constexpr uint32_t kPositionFloats = 3;
constexpr uint32_t kPairFloats = 2;
constexpr uint32_t kQuadFloats = 4;
constexpr uint32_t kSphereFloats = 12;
constexpr uint32_t kArgWords = 5;
constexpr uint32_t kBatchWords = 2;
constexpr auto kIndexUse = Native::BufferUse::Index | Native::BufferUse::Storage;

struct RebasedRun {
  const uint32_t *From = nullptr;
  uint32_t Count = 0;
  uint32_t FirstVertex = 0;
};

void WriteRebased(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const RebasedRun *>(carrying);
  auto *const words = reinterpret_cast<uint32_t *>(into);
  for (uint32_t at = 0; at < floats && at < held->Count; ++at) {
    words[at] = held->From[at] + held->FirstVertex;
  }
}

struct PieceRun {
  const PieceMesh *Piece = nullptr;
};

void WritePiecePositions(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPositionFloats > floats) { break; }
    into[at++] = one.pos[0];
    into[at++] = one.pos[1];
    into[at++] = one.pos[2];
  }
}

void WritePieceNormals(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at == floats) { break; }
    std::memcpy(into + at++, &one.normWord, sizeof(one.normWord));
  }
}

void WritePieceEmission(const void *carrying, float *into, uint32_t floats) {
  const auto &colour = *static_cast<const std::array<float, 3> *>(carrying);
  for (uint32_t at = 0; at < floats; ++at) { into[at] = colour[at % colour.size()]; }
}

void WritePieceUv(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPairFloats > floats) { break; }
    const Vec2f uv = one.uv();
    into[at++] = uv[0];
    into[at++] = uv[1];
  }
}

struct PieceAllocation {
  SubjectResidency::Range Vertices, Indices, Colours, Tangents;
};

bool UploadPieceRuns(SubjectResidency &res,
                     const PieceMesh &piece,
                     const PieceAllocation &allocation,
                     std::string &error) {
  const auto verts = static_cast<uint32_t>(piece.Verts.size());
  const auto indices = static_cast<uint32_t>(piece.Indices.size());
  const auto &v = allocation.Vertices;
  const auto &i = allocation.Indices;
  const auto &c = allocation.Colours;
  const auto &t = allocation.Tangents;
  const PieceRun carrying{.Piece = &piece};
  const RebasedRun rebased{.From = piece.Indices.data(), .Count = indices, .FirstVertex = v.First};
  const auto floatsAt = [v](uint32_t wide) {
    return v.First * wide * static_cast<uint32_t>(sizeof(float));
  };
  const auto vertexUse = Native::BufferUse::Vertex;
  const uint32_t positionBytes = verts * kPositionFloats * static_cast<uint32_t>(sizeof(float));
  std::array<SubjectResidency::Crossing, 7> crossings = {{
      {.Which = SubjectResidency::Stream::Vertex,
       .Usage = vertexUse,
       .Bytes = positionBytes,
       .Offset = floatsAt(kPositionFloats),
       .Writes = WritePiecePositions,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Normal,
       .Usage = vertexUse,
       .Bytes = verts * static_cast<uint32_t>(sizeof(uint32_t)),
       .Offset = floatsAt(1u),
       .Writes = WritePieceNormals,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Index,
       .Usage = kIndexUse,
       .Bytes = indices * static_cast<uint32_t>(sizeof(uint32_t)),
       .Offset = i.First * static_cast<uint32_t>(sizeof(uint32_t)),
       .Writes = WriteRebased,
       .Carrying = &rebased},
      {.Which = SubjectResidency::Stream::Uv,
       .Usage = vertexUse,
       .Bytes = piece.Textured ? verts * kPairFloats * static_cast<uint32_t>(sizeof(float)) : 0u,
       .Offset = floatsAt(kPairFloats),
       .Writes = WritePieceUv,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Uv1,
       .Usage = vertexUse,
       .From = piece.Uv1.data(),
       .Bytes = static_cast<uint32_t>(piece.Uv1.size() * sizeof(float)),
       .Offset = floatsAt(kPairFloats)},
      {.Which = SubjectResidency::Stream::Tangent,
       .Usage = Native::BufferUse::Storage,
       .From = piece.Tangents.data(),
       .Bytes = static_cast<uint32_t>(piece.Tangents.size() * sizeof(float)),
       .Offset = t.First * kQuadFloats * static_cast<uint32_t>(sizeof(float))},
      {.Which = SubjectResidency::Stream::Colour,
       .Usage = Native::BufferUse::Storage,
       .From = piece.Colours.data(),
       .Bytes = static_cast<uint32_t>(piece.Colours.size() * sizeof(float)),
       .Offset = c.First * kQuadFloats * static_cast<uint32_t>(sizeof(float))},
  }};
  size_t count = 0;
  for (const SubjectResidency::Crossing &one : crossings) {
    if (one.Bytes > 0 && one.Stands()) { crossings[count++] = one; }
  }
  return Uploaded(res.Cross(std::span<SubjectResidency::Crossing>(crossings.data(), count), true),
                  error);
}

}

bool SubjectDraw::RoomForStreams(std::string &error) {
  SubjectResidency &res = Bound();
  const uint32_t verts = res.VertexRoom();
  const uint32_t subject = res.SubjectVertices().First + res.SubjectVertices().Count;
  const auto bytes = [](uint32_t count, uint32_t wide) {
    return count * wide * static_cast<uint32_t>(sizeof(float));
  };
  const auto vertex = Native::BufferUse::Vertex;
  using S = SubjectResidency::Stream;
  return Uploaded(
             res.Grow(S::Vertex,
                      {.Usage = vertex, .Bytes = bytes(verts, kPositionFloats), .Deferred = true}),
             error) &&
         Uploaded(
             res.Grow(S::Normal, {.Usage = vertex, .Bytes = bytes(verts, 1u), .Deferred = true}),
             error) &&
         (!res.Shape().HasPrevious || subject == 0 ||
          Uploaded(
              res.Grow(
                  S::Previous,
                  {.Usage = vertex, .Bytes = bytes(subject, kPositionFloats), .Deferred = true}),
              error)) &&
         (subject == 0 ||
          ((!res.Shape().HasEmitted || Uploaded(res.Grow(S::Emitted,
                                                         {.Usage = vertex,
                                                          .Bytes = bytes(subject, kPositionFloats),
                                                          .Deferred = true}),
                                                error)) &&
           RoomForOptionalStreams(
               {.VertexEnd = res.SubjectVertices().First + res.Shape().Vertices,
                .ColourEnd = res.SubjectColours().First + res.Shape().Vertices,
                .TangentEnd = res.SubjectTangents().First + res.Shape().Vertices},
               {.Uv = res.Shape().HasUv,
                .Uv1 = res.Shape().HasUv1,
                .Tangent = res.Shape().HasTangent,
                .Colour = res.Shape().HasColour},
               error))) &&
         Uploaded(res.Grow(S::Index,
                           {.Usage = kIndexUse,
                            .Bytes = res.IndexRoom() * static_cast<uint32_t>(sizeof(uint32_t)),
                            .Deferred = true}),
                  error);
}

bool SubjectDraw::RoomForOptionalStreams(OptionalStreamEnds ends,
                                         VertexRunsCarried carried,
                                         std::string &error) {
  using S = SubjectResidency::Stream;
  const auto grow = [&](S stream, bool present, uint32_t components) {
    return !present || Uploaded(Bound().Grow(stream,
                                             {.Usage = Native::BufferUse::Vertex,
                                              .Bytes = ends.VertexEnd * components *
                                                       static_cast<uint32_t>(sizeof(float)),
                                              .Deferred = true}),
                                error);
  };
  return grow(S::Uv, carried.Uv, kPairFloats) && grow(S::Uv1, carried.Uv1, kPairFloats) &&
         (!carried.Tangent || Uploaded(Bound().Grow(S::Tangent,
                                                    {.Usage = Native::BufferUse::Storage,
                                                     .Bytes = ends.TangentEnd * kQuadFloats *
                                                              static_cast<uint32_t>(sizeof(float)),
                                                     .Deferred = true}),
                                       error)) &&
         (!carried.Colour || Uploaded(Bound().Grow(S::Colour,
                                                   {.Usage = Native::BufferUse::Storage,
                                                    .Bytes = ends.ColourEnd * kQuadFloats *
                                                             static_cast<uint32_t>(sizeof(float)),
                                                    .Deferred = true}),
                                      error));
}

bool SubjectDraw::ValidateBatch(const SubjectMesh &mesh,
                                const DrawBatch &batch,
                                std::string &error) const {
  if (static_cast<size_t>(batch.Layout) >= kVertexLayouts.size()) {
    error = Says::InvalidMeshLayout;
    return false;
  }
  if ((CarriesUv(batch.Layout) && !mesh.Uv.Stands()) ||
      (CarriesUv1(batch.Layout) && !mesh.Uv1.Stands()) ||
      (CarriesNormal(batch.Layout) && !mesh.Normals.Stands()) ||
      (CarriesTangent(batch.Layout) && !mesh.Tangents.Stands()) ||
      (CarriesColour(batch.Layout) && !mesh.Colours.Stands())) {
    error = Says::MissingMeshAttributes;
    return false;
  }
  if (uint64_t{batch.ModelSlot} + batch.Instances > std::numeric_limits<uint32_t>::max()) {
    error = Says::InvalidMeshInstances;
    return false;
  }
  if (batch.MaterialSlot < Slots.size() && Slots[batch.MaterialSlot].ReadsSecondUv &&
      !(CarriesUv1(batch.Layout) && mesh.Uv1.Stands())) {
    error = "surface slot " + std::to_string(batch.MaterialSlot) +
            " requires the second UV set and the draw wearing it " +
            (mesh.Uv1.Stands() ? "takes a vertex layout that binds no second run"
                               : "has no second uv run at all") +
            ", and the first set is not a substitute for it";
    return false;
  }
  if (batch.MaterialSlot >= Slots.size()) {
    error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
            " over a table of " + std::to_string(Slots.size()) + " surfaces";
    return false;
  }
  if (uint64_t{batch.FirstIndex} + batch.IndexCount > mesh.IndexCount) {
    error = "a draw covers indices " + std::to_string(batch.FirstIndex) + " to " +
            std::to_string(uint64_t{batch.FirstIndex} + batch.IndexCount) + " over a run of " +
            std::to_string(mesh.IndexCount);
    return false;
  }
  return true;
}

bool SubjectDraw::ValidateMesh(const SubjectMesh &mesh, std::string &error) const {
  if (mesh.VertexCount == 0 || mesh.IndexCount == 0) { return true; }
  if (Binding().Device == nullptr) {
    error = "the subject stage carries no device, so a mesh of " +
            std::to_string(mesh.VertexCount) + " vertices has nowhere to become resident";
    return false;
  }
  {
    const char *missing = nullptr;
    if (!mesh.Verts.Stands()) {
      missing = "position run";
    } else if (mesh.Indices == nullptr) {
      missing = "index run";
    } else if (mesh.Draws == nullptr) {
      missing = "draw list";
    } else if (!mesh.Emitted.Stands()) {
      missing = "emitted-radiance run";
    }
    if (missing != nullptr) {
      error = std::string("the mesh declares ") + std::to_string(mesh.VertexCount) +
              " vertices and " + std::to_string(mesh.IndexCount) + " indices but carries no " +
              missing +
              " -- a declaration that names geometry it does not hand over draws "
              "nothing, and drawing nothing is not what it asked for";
      return false;
    }
  }

  if (mesh.PrevVerts.Stands() && !Binding().WritesVelocity) {
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
    if (!ValidateBatch(mesh, batch, error)) { return false; }
  }
  return true;
}

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {
  auto began = BeginMesh(mesh);
  if (!began) {
    error = std::move(began.error());
    return false;
  }
  return FinishMesh(*began, mesh, error);
}

std::expected<SubjectDraw::MeshTicket, std::string>
SubjectDraw::BeginMesh(const SubjectMesh &mesh) {
  std::string error;
  LastMeshUpload_ = {};
  const auto admissionAt = std::chrono::steady_clock::now();
  if (!ValidateMesh(mesh, error) || !Uploaded(Bound().SubmitPendingUploads(), error)) {
    return std::unexpected(std::move(error));
  }
  ++Reshaped_;
  PendingMesh_ = {};
  PendingDraws_ = nullptr;
  PendingIndices_ = nullptr;
  PendingIndexCount_ = 0;
  Bound().Shape().Vertices = mesh.VertexCount;
  Bound().Shape().Indices = mesh.IndexCount;
  Bound().Shape().HasUv = mesh.Uv.Stands();
  Bound().Shape().HasUv1 = mesh.Uv1.Stands();
  Bound().Shape().HasNormal = mesh.Normals.Stands();
  Bound().Shape().HasTangent = mesh.Tangents.Stands();
  Bound().Shape().HasColour = mesh.Colours.Stands();
  Bound().Shape().HasEmitted =
      mesh.Draws != nullptr &&
      std::ranges::any_of(mesh.Draws->Batches(),
                          [](const DrawBatch &batch) { return !CarriesNormal(batch.Layout); });
  Bound().Shape().HasPrevious = Binding().WritesVelocity && mesh.Draws != nullptr &&
                                UsesPrevious(mesh, mesh.Draws->Batches());
  SubjectBatches_.clear();
  SubjectJobs_.clear();
  SubjectSpheres_.clear();
  SubjectRows_ = 0;
  TablesStale_ = true;
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = mesh.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    ModelBefore_ = ModelStamp_ == 0u ? mesh.Model : Model;
    ModelStamp_ = Frame_;
  }
  for (int part = 0; part < 16; part++) { Model[part] = mesh.Model[part]; }
  RowsStale_ = true;
  if (Bound().Shape().Vertices == 0 || Bound().Shape().Indices == 0) {
    Bound().Shape().Indices = 0;
    if (!Borrows()) {
      Bound().GiveVertices(Bound().SubjectVertices());
      Bound().GiveIndices(Bound().SubjectIndices());
      Bound().GiveColours(Bound().SubjectColours());
      Bound().GiveTangents(Bound().SubjectTangents());
      Bound().SubjectStands({}, {}, {}, {});
    }
    if (!HandTables(error)) { return std::unexpected(std::move(error)); }
    return MeshTicket{.Generation = Reshaped_};
  }
  SubjectBatches_ = mesh.Draws->Batches();
  for (const DrawBatch &batch : SubjectBatches_) {
    SubjectRows_ = std::max(SubjectRows_, batch.ModelSlot + batch.Instances);
  }

  if (Borrows()) {
    if (!HandTables(error)) { return std::unexpected(std::move(error)); }
    return MeshTicket{.Generation = Reshaped_};
  }

  LastMeshUpload_.AdmissionMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - admissionAt)
          .count();
  const auto indexAt = std::chrono::steady_clock::now();
  {
    static const Heap::Tag kUploadingTag("mesh-upload");
    const Heap::Tagged uploading(kUploadingTag);
    Bound().GiveVertices(Bound().SubjectVertices());
    Bound().GiveIndices(Bound().SubjectIndices());
    Bound().GiveColours(Bound().SubjectColours());
    Bound().GiveTangents(Bound().SubjectTangents());
    const SubjectResidency::Range v = Bound().TakeVertices(mesh.VertexCount);
    const SubjectResidency::Range i = Bound().TakeIndices(mesh.IndexCount);
    const auto c =
        mesh.Colours.Stands() ? Bound().TakeColours(mesh.VertexCount) : SubjectResidency::Range{};
    const auto t =
        mesh.Tangents.Stands() ? Bound().TakeTangents(mesh.VertexCount) : SubjectResidency::Range{};
    Bound().SubjectStands(v, i, c, t);
    if (!RoomForStreams(error)) {
      Bound().Shape().Indices = 0;
      return std::unexpected(std::move(error));
    }
    const RebasedRun rebased{
        .From = mesh.Indices, .Count = mesh.IndexCount, .FirstVertex = v.First};
    std::array run = {SubjectResidency::Crossing{
        .Which = SubjectResidency::Stream::Index,
        .Usage = kIndexUse,
        .Bytes = mesh.IndexCount * static_cast<uint32_t>(sizeof(uint32_t)),
        .Offset = i.First * static_cast<uint32_t>(sizeof(uint32_t)),
        .Writes = WriteRebased,
        .Carrying = &rebased}};
    if (!Uploaded(Bound().Cross(run, true), error)) {
      Bound().Shape().Indices = 0;
      return std::unexpected(std::move(error));
    }
  }
  if (!Bound().Buffer(SubjectResidency::Stream::Index)) {
    Bound().Shape().Indices = 0;
    error = std::string("the subject's index run did not reach the device: ");
    return std::unexpected(std::move(error));
  }
  LastMeshUpload_.IndexMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - indexAt).count();
  PendingMesh_ = {.Generation = Reshaped_, .NeedsFinish = true};
  PendingDraws_ = mesh.Draws;
  PendingIndices_ = mesh.Indices;
  PendingIndexCount_ = mesh.IndexCount;
  Bound().Shape().Indices = 0;
  return PendingMesh_;
}

bool SubjectDraw::FinishMesh(MeshTicket ticket, const SubjectMesh &mesh, std::string &error) {
  if (!ticket.NeedsFinish) {
    if (ticket.Generation != 0 && ticket.Generation == Reshaped_ && !PendingMesh_.NeedsFinish) {
      return true;
    }
    error = "subject mesh completion has no matching preparation";
    return false;
  }
  if (ticket.Generation == 0 || ticket.Generation != PendingMesh_.Generation ||
      mesh.Draws != PendingDraws_ || mesh.Indices != PendingIndices_ ||
      mesh.IndexCount != PendingIndexCount_ || mesh.VertexCount != Bound().Shape().Vertices) {
    error = "subject mesh completion has no matching preparation";
    return false;
  }
  Bound().Shape().Indices = PendingIndexCount_;
  PendingMesh_ = {};
  PendingDraws_ = nullptr;
  PendingIndices_ = nullptr;
  PendingIndexCount_ = 0;
  const auto streamsAt = std::chrono::steady_clock::now();
  if (!HandStreams(mesh, true, error)) { return false; }
  LastMeshUpload_.StreamsMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - streamsAt)
          .count();

  const auto tablesAt = std::chrono::steady_clock::now();
  const std::vector<uint32_t> &jobs = mesh.Draws->ClusterJobs();
  if (!jobs.empty() && !mesh.ClusterSpheres.empty()) {
    SubjectJobs_ = jobs;
    SubjectSpheres_.assign(mesh.ClusterSpheres.begin(), mesh.ClusterSpheres.end());
  }
  const bool handed = HandTables(error);
  LastMeshUpload_.TablesMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tablesAt)
          .count();
  if (!handed) { Bound().Shape().Indices = 0; }
  return handed;
}

bool SubjectDraw::HandStreams(const SubjectPose &pose, bool deferred, std::string &error) {
  static const Heap::Tag kUploadingTag("mesh-upload");
  const Heap::Tagged uploading(kUploadingTag);
  const SubjectResidency::Range vertices{.First = Bound().SubjectVertices().First,
                                         .Count = Bound().Shape().Vertices};
  Bound().Shape().HasPrevious = Binding().WritesVelocity && UsesPrevious(pose, SubjectBatches_);
  using Stream = SubjectResidency::Stream;
  const NormalUpload normals{.Source = pose.Normals};
  const std::array<VertexStreamUpload, 8> uploads{
      {{.Which = Stream::Vertex,
        .Source = pose.Verts,
        .Carried = true,
        .Components = kPositionFloats},
       {.Which = Stream::Emitted,
        .Source = pose.Emitted,
        .Carried = Bound().Shape().HasEmitted,
        .Components = kPositionFloats},
       {.Which = Stream::Normal,
        .Source = pose.Normals.Stands()
                      ? SubjectStream{.Writes = NormalUpload::Write, .Carrying = &normals}
                      : SubjectStream{},
        .Carried = Bound().Shape().HasNormal,
        .Components = 1},
       {.Which = Stream::Tangent,
        .Source = pose.Tangents,
        .Carried = Bound().Shape().HasTangent,
        .Components = kQuadFloats},
       {.Which = Stream::Uv,
        .Source = pose.Uv,
        .Carried = Bound().Shape().HasUv,
        .Components = kPairFloats},
       {.Which = Stream::Uv1,
        .Source = pose.Uv1,
        .Carried = Bound().Shape().HasUv1,
        .Components = kPairFloats},
       {.Which = Stream::Colour,
        .Source = pose.Colours,
        .Carried = Bound().Shape().HasColour,
        .Components = kQuadFloats},
       {.Which = Stream::Previous,
        .Source = pose.PrevVerts,
        .Carried = Bound().Shape().HasPrevious,
        .Components = kPositionFloats}}};
  std::array<SubjectResidency::Crossing, uploads.size()> streams;
  size_t count = 0;
  for (const auto &upload : uploads) {
    auto range = vertices;
    if (upload.Which == Stream::Colour) { range.First = Bound().SubjectColours().First; }
    if (upload.Which == Stream::Tangent) { range.First = Bound().SubjectTangents().First; }
    const auto crossing = VertexCrossing(upload, range);
    if (!crossing) {
      error = crossing.error();
      return false;
    }
    if (crossing->Bytes > 0 && crossing->Stands()) { streams[count++] = *crossing; }
  }
  if (!HandPlacements(deferred, error)) { return false; }
  if (!Uploaded(
          Bound().Cross(std::span<SubjectResidency::Crossing>(streams.data(), count), deferred),
          error)) {
    Bound().Shape().Indices = 0;
    return false;
  }
  if (!Bound().Buffer(SubjectResidency::Stream::Vertex) ||
      (Bound().Shape().HasEmitted && !Bound().Buffer(SubjectResidency::Stream::Emitted)) ||
      (Bound().Shape().HasPrevious && !Bound().Buffer(SubjectResidency::Stream::Previous)) ||
      (Bound().Shape().HasColour && !Bound().Buffer(SubjectResidency::Stream::Colour))) {
    Bound().Shape().Indices = 0;
    error = std::string("the subject's vertex streams did not reach the device: ");
    return false;
  }
  return true;
}

bool SubjectDraw::ValidatePiece(const PieceMesh &piece, std::string &error) const {
  if (Borrows()) { return false; }
  if (!ValidPieceUv1(piece)) {
    error = kPieceUv1Error;
    return false;
  }
  if (piece.MaxInstances > 0 && std::max(size_t{1}, piece.Instances.size()) > piece.MaxInstances) {
    error = Says::PieceInstanceLimit;
    return false;
  }
  if (piece.Verts.empty() || piece.Indices.size() < 3 || piece.Indices.size() % 3 != 0) {
    error = "a piece of " + std::to_string(piece.Verts.size()) + " vertices and " +
            std::to_string(piece.Indices.size()) +
            " indices is not a mesh, and placing nothing is not what was asked for";
    return false;
  }
  if (!piece.Colours.empty() && piece.Colours.size() != piece.Verts.size() * kQuadFloats) {
    error = "a piece carries " + std::to_string(piece.Colours.size() / kQuadFloats) +
            " colours over " + std::to_string(piece.Verts.size()) + " vertices";
    return false;
  }
  if (!piece.Tangents.empty() &&
      (piece.Tangents.size() != piece.Verts.size() * kQuadFloats || !piece.Textured)) {
    error = "piece tangents require UVs and one float4 per vertex";
    return false;
  }
  if (Binding().Device == nullptr) {
    error = "the subject stage carries no device, so a piece has nowhere to become resident";
    return false;
  }
  const std::span<const uint32_t> slots = piece.Surface.From == PieceSurface::Source::Registered
                                              ? std::span<const uint32_t>(RegisteredSlotOf_)
                                              : std::span<const uint32_t>(SlotOf_);
  if (piece.Surface.Index >= slots.size() || slots[piece.Surface.Index] == kNoSlot) {
    error = "a piece refers to " +
            std::string(piece.Surface.From == PieceSurface::Source::Registered ? "a registered"
                                                                               : "a native") +
            " surface " + std::to_string(piece.Surface.Index) + " but the subject has " +
            std::to_string(slots.size()) + " surface slots";
    return false;
  }
  if (Slots[slots[piece.Surface.Index]].ReadsSecondUv && piece.Uv1.empty()) {
    error = "piece surface requires a complete second UV run";
    return false;
  }
  return true;
}

PieceId SubjectDraw::PlacePiece(const PieceMesh &piece, std::string &error) {
  if (!ValidatePiece(piece, error)) { return kNoPiece; }
  static const Heap::Tag kUploadingTag("mesh-upload");
  const Heap::Tagged uploading(kUploadingTag);
  const auto verts = static_cast<uint32_t>(piece.Verts.size());
  const auto indices = static_cast<uint32_t>(piece.Indices.size());
  SubjectResidency &res = Bound();
  const SubjectResidency::Range v = res.TakeVertices(verts);
  const SubjectResidency::Range i = res.TakeIndices(indices);
  const auto c = piece.Colours.empty() ? SubjectResidency::Range{} : res.TakeColours(verts);
  const auto t = piece.Tangents.empty() ? SubjectResidency::Range{} : res.TakeTangents(verts);
  const auto giveBack = [&res, v, i, c, t] {
    res.GiveVertices(v);
    res.GiveIndices(i);
    res.GiveColours(c);
    res.GiveTangents(t);
  };
  if (!RoomForStreams(error) || !RoomForOptionalStreams({.VertexEnd = v.First + verts,
                                                         .ColourEnd = c.First + verts,
                                                         .TangentEnd = t.First + verts},
                                                        {.Uv = piece.Textured,
                                                         .Uv1 = !piece.Uv1.empty(),
                                                         .Tangent = !piece.Tangents.empty(),
                                                         .Colour = !piece.Colours.empty()},
                                                        error)) {
    giveBack();
    return kNoPiece;
  }
  if (!UploadPieceRuns(
          res, piece, {.Vertices = v, .Indices = i, .Colours = c, .Tangents = t}, error)) {
    giveBack();
    return kNoPiece;
  }

  PieceId id = kNoPiece;
  if (!Spare_.empty()) {
    id = Spare_.back();
    Spare_.pop_back();
  } else {
    id = static_cast<PieceId>(Pieces_.size());
    Pieces_.emplace_back();
  }
  Piece &held = Pieces_[id];
  held.V = v;
  held.I = i;
  held.C = c;
  held.T = t;
  held.IndexCount = indices;
  held.Surface = piece.Surface;
  held.Emitted.reset();
  held.MaxInstances = piece.MaxInstances;
  held.Rows.reserve(piece.MaxInstances);
  if (piece.Instances.empty()) {
    held.Rows.assign(1, piece.Row);
  } else {
    held.Rows.assign(piece.Instances.begin(), piece.Instances.end());
  }
  held.Clusters.assign(piece.Clusters.begin(), piece.Clusters.end());
  held.LocalSphere = held.Clusters.empty() ? LocalSphereOf(piece.Verts) : std::nullopt;
  held.WorldSphere = WorldSphereOf(held.LocalSphere, held.Rows);
  VertexRunsCarried carried;
  carried.Normal = true;
  carried.Uv = piece.Textured;
  carried.Uv1 = !piece.Uv1.empty();
  carried.Tangent = !piece.Tangents.empty();
  carried.Colour = !piece.Colours.empty();
  (void)LayoutOf(carried, held.Layout);
  held.Live = true;
  ++PiecesLive_;
  PieceTriangles_ += indices / 3u;
  TablesStale_ = true;
  RowsStale_ = true;
  ++Reshaped_;
  return id;
}

bool SubjectDraw::SetPieceInstances(PieceId which, std::span<const Mat4> rows, std::string &error) {
  if (which >= Pieces_.size() || !Pieces_[which].Live) {
    error = Says::MissingPiece;
    return false;
  }
  Piece &piece = Pieces_[which];
  if (piece.MaxInstances > 0 && rows.size() > piece.MaxInstances) {
    error = Says::PieceInstanceLimit;
    return false;
  }
  if (std::ranges::equal(piece.Rows, rows)) { return true; }
  piece.Rows.assign(rows.begin(), rows.end());
  piece.WorldSphere = WorldSphereOf(piece.LocalSphere, piece.Rows);
  RowsStale_ = TablesStale_ = true;
  ++Reshaped_;
  return true;
}

void SubjectDraw::ReleasePiece(PieceId which) {
  if (which >= Pieces_.size() || !Pieces_[which].Live) { return; }
  Piece &held = Pieces_[which];
  Bound().GiveVertices(held.V);
  Bound().GiveIndices(held.I);
  Bound().GiveColours(held.C);
  Bound().GiveTangents(held.T);
  PieceTriangles_ -= held.IndexCount / 3u;
  held.Live = false;
  held.Clusters.clear();
  held.Rows.clear();
  Spare_.push_back(which);
  --PiecesLive_;
  TablesStale_ = true;
  RowsStale_ = true;
  ++Reshaped_;
}

void SubjectDraw::SetPiecesVisible(bool visible) {
  if (PiecesVisible_ == visible) { return; }
  PiecesVisible_ = visible;
  TablesStale_ = true;
  ++Reshaped_;
}

void SubjectDraw::SetNativePieceSurfaces(std::span<const uint32_t> slots) {
  if (std::ranges::equal(SlotOf_, slots)) { return; }
  SlotOf_.assign(slots.begin(), slots.end());
  TablesStale_ = true;
}

void SubjectDraw::SetRegisteredPieceSurfaces(std::span<const uint32_t> slots) {
  if (std::ranges::equal(RegisteredSlotOf_, slots)) { return; }
  RegisteredSlotOf_.assign(slots.begin(), slots.end());
  TablesStale_ = true;
}

bool SubjectDraw::HandTables(std::string &error) {
  if (!TablesStale_) { return true; }
  if (!Retable(error)) {
    Args_.clear();
    Jobs_ = 0;
    return false;
  }
  TablesStale_ = false;
  ++Reshaped_;
  return true;
}

bool SubjectDraw::Retable(std::string &error) {
  static const Heap::Tag kUploadingTag("mesh-cull");
  const Heap::Tagged uploading(kUploadingTag);
  Batches.clear();
  BatchLayout.clear();
  Args_.clear();
  Jobs_ = 0;
  if (!BuildSubjectBatches(error)) { return false; }
  if (Borrows()) { return true; }
  PrepareSubjectTables();
  OrderPieces();
  for (const uint32_t at : TableOrder_) {
    if (!AppendPieceBatches(Pieces_[at], error)) { return false; }
  }
  return UploadTables(error);
}

bool SubjectDraw::BuildSubjectBatches(std::string &error) {
  const uint32_t subjectIndexFirst = Bound().SubjectIndices().First;
  for (const DrawBatch &one : SubjectBatches_) {
    DrawBatch batch = one;
    batch.FirstIndex += subjectIndexFirst;
    VertexRunsCarried carried;
    carried.Uv = CarriesUv(batch.Layout) && Bound().Shape().HasUv;
    carried.Normal = CarriesNormal(batch.Layout) && Bound().Shape().HasNormal;
    carried.Tangent =
        CarriesTangent(batch.Layout) && carried.Normal && carried.Uv && Bound().Shape().HasTangent;
    carried.Uv1 = CarriesUv1(batch.Layout) && carried.Uv && Bound().Shape().HasUv1;
    carried.Colour = CarriesColour(batch.Layout) && Bound().Shape().HasColour;
    VertexLayout drawn = VertexLayout::Position;
    if (!LayoutOf(carried, drawn)) {
      Bound().Shape().Indices = 0;
      Batches.clear();
      BatchLayout.clear();
      error = "a draw's runs name no vertex layout this engine builds, and the nearest one is not "
              "an answer -- the combination is what the enumeration exists to refuse";
      return false;
    }
    Batches.push_back(batch);
    BatchLayout.push_back(drawn);
  }
  return true;
}

void SubjectDraw::PrepareSubjectTables() {
  const uint32_t subjectIndexFirst = Bound().SubjectIndices().First;
  std::vector<uint32_t> &jobs = TableJobs_;
  std::vector<float> &spheres = TableSpheres_;
  jobs.assign(SubjectJobs_.begin(), SubjectJobs_.end());
  spheres.assign(SubjectSpheres_.begin(), SubjectSpheres_.end());
  for (size_t at = 2; at < jobs.size(); at += DrawList::kJobWords) {
    jobs[at] += subjectIndexFirst;
  }
  const bool subjectCut = !jobs.empty() && !spheres.empty();
  if (!subjectCut) {
    jobs.clear();
    spheres.clear();
    for (DrawBatch &batch : Batches) {
      batch.FirstJob = 0;
      batch.JobCount = 0;
    }
  }
}

uint32_t SubjectDraw::MaterialSlotFor(const Piece &piece) const {
  const auto &slots =
      piece.Surface.From == PieceSurface::Source::Registered ? RegisteredSlotOf_ : SlotOf_;
  return piece.Surface.Index < slots.size() ? slots[piece.Surface.Index] : kNoSlot;
}

void SubjectDraw::OrderPieces() {
  auto &jobs = TableJobs_;
  auto &spheres = TableSpheres_;
  std::vector<uint32_t> &order = TableOrder_;
  order.clear();
  size_t clusters = 0;
  size_t uniqueClusters = 0;
  auto nextRow =
      static_cast<uint32_t>(std::max(Placed_.size() / 16u, static_cast<size_t>(SubjectRows_)));
  for (Piece &piece : Pieces_) {
    piece.FirstRow = nextRow;
    nextRow += static_cast<uint32_t>(piece.Rows.size());
  }
  for (uint32_t at = 0; at < Pieces_.size(); ++at) {
    if (Pieces_[at].Live && !Pieces_[at].Rows.empty() && MaterialSlotFor(Pieces_[at]) != kNoSlot &&
        MaterialSlotFor(Pieces_[at]) < Slots.size()) {
      order.push_back(at);
      clusters += Pieces_[at].Clusters.size() * Pieces_[at].Rows.size();
      uniqueClusters += Pieces_[at].Clusters.size();
    }
  }
  jobs.reserve(jobs.size() + clusters * DrawList::kJobWords);
  spheres.reserve(spheres.size() + uniqueClusters * kSphereFloats);
  std::ranges::sort(order, [this](uint32_t a, uint32_t b) {
    const Piece &left = Pieces_[a];
    const Piece &right = Pieces_[b];
    if (MaterialSlotFor(left) != MaterialSlotFor(right)) {
      return MaterialSlotFor(left) < MaterialSlotFor(right);
    }
    if (left.Layout != right.Layout) { return left.Layout < right.Layout; }
    return a < b;
  });
}

bool SubjectDraw::AppendPieceBatches(Piece &one, std::string &error) {
  if (!PiecesVisible_) { return true; }
  auto &jobs = TableJobs_;
  auto &spheres = TableSpheres_;
  VertexLayout layout = one.Layout;
  const auto &unlit = Slots[MaterialSlotFor(one)].Unlit;
  if (unlit) {
    if (one.Emitted != unlit) {
      using S = SubjectResidency::Stream;
      const uint32_t stride = kPositionFloats * static_cast<uint32_t>(sizeof(float));
      if (!Uploaded(Bound().Grow(S::Emitted,
                                 {.Usage = Native::BufferUse::Vertex,
                                  .Bytes = (one.V.First + one.V.Count) * stride,
                                  .Deferred = true}),
                    error)) {
        return false;
      }
      SubjectResidency::Crossing crossing{.Which = S::Emitted,
                                          .Usage = Native::BufferUse::Vertex,
                                          .Bytes = one.V.Count * stride,
                                          .Offset = one.V.First * stride,
                                          .Writes = WritePieceEmission,
                                          .Carrying = &*unlit};
      if (!Uploaded(Bound().Cross(std::span(&crossing, 1), true), error)) { return false; }
      one.Emitted = unlit;
    }
    VertexRunsCarried carried;
    carried.Uv = CarriesUv(layout);
    carried.Uv1 = CarriesUv1(layout);
    carried.Colour = CarriesColour(layout);
    (void)LayoutOf(carried, layout);
  }
  const auto firstSphere = static_cast<uint32_t>(spheres.size() / kSphereFloats);
  for (const DagCluster &cluster : one.Clusters) {
    spheres.insert(spheres.end(),
                   {cluster.SelfCenter[0],
                    cluster.SelfCenter[1],
                    cluster.SelfCenter[2],
                    cluster.SelfRadius,
                    cluster.ParentCenter[0],
                    cluster.ParentCenter[1],
                    cluster.ParentCenter[2],
                    cluster.ParentRadius,
                    cluster.SelfErr,
                    cluster.ParentErr,
                    0.0f,
                    0.0f});
  }
  const size_t runs = one.Clusters.empty() ? 1u : one.Rows.size();
  for (size_t instance = 0; instance < runs; ++instance) {
    const auto row = static_cast<uint32_t>(Batches.size());
    DrawBatch batch{};
    batch.Motion = VertexMotion::Rigid;
    batch.WorldSphere = one.WorldSphere;
    batch.FirstIndex = one.I.First;
    batch.IndexCount = one.IndexCount;
    batch.MaterialSlot = MaterialSlotFor(one);
    batch.Layout = layout;
    batch.Kind = Slots[batch.MaterialSlot].Kind;
    batch.Draws = 1;
    batch.ModelSlot = one.FirstRow + static_cast<uint32_t>(instance);
    batch.Instances = one.Clusters.empty() ? static_cast<uint32_t>(one.Rows.size()) : 1u;
    batch.FirstJob = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords);
    auto sphere = firstSphere;
    for (const DagCluster &cluster : one.Clusters) {
      jobs.insert(jobs.end(), {sphere++, row, one.I.First + cluster.First, cluster.Count});
    }
    batch.JobCount = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords) - batch.FirstJob;
    Batches.push_back(batch);
    BatchLayout.push_back(layout);
  }
  return true;
}

bool SubjectDraw::UploadTables(std::string &error) {
  auto &jobs = TableJobs_;
  auto &spheres = TableSpheres_;
  if (jobs.empty() || spheres.empty()) { return true; }
  Args_.assign(Batches.size() * kArgWords, 0u);
  std::vector<uint32_t> &rows = TableRows_;
  rows.assign(Batches.size() * kBatchWords, 0u);
  uint32_t base = 0;
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    Args_[at * kArgWords + 1u] = batch.Instances;
    Args_[at * kArgWords + 2u] = base;
    Args_[at * kArgWords + 4u] = batch.ModelSlot;
    rows[at * kBatchWords] = batch.FirstJob;
    rows[at * kBatchWords + 1u] = batch.JobCount;
    for (uint32_t one = 0; one < batch.JobCount; ++one) {
      base += jobs[(static_cast<size_t>(batch.FirstJob) + one) * DrawList::kJobWords + 3u];
    }
  }
  Jobs_ = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords);
  if (base == 0) {
    Args_.clear();
    Jobs_ = 0;
    return true;
  }

  const auto read = Native::BufferUse::Storage;
  std::array cut = {
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterSpheres,
                                 .Usage = read,
                                 .From = spheres.data(),
                                 .Bytes = static_cast<uint32_t>(spheres.size() * sizeof(float))},
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterJobs,
                                 .Usage = read,
                                 .From = jobs.data(),
                                 .Bytes = static_cast<uint32_t>(jobs.size() * sizeof(uint32_t))},
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterBatches,
                                 .Usage = read,
                                 .From = rows.data(),
                                 .Bytes = static_cast<uint32_t>(rows.size() * sizeof(uint32_t))},
  };
  if (!Uploaded(Bound().Cross(cut, true), error)) {
    Args_.clear();
    Jobs_ = 0;
    return false;
  }

  constexpr auto kCullUse = Native::BufferUse::Storage;
  const uint32_t jobBytes = Jobs_ * static_cast<uint32_t>(sizeof(uint32_t));
  constexpr auto discard = SubjectResidency::ExistingContents::Discard;
  if (!Uploaded(Bound().Grow(
                    SubjectResidency::Stream::ClusterKept,
                    {.Usage = kCullUse, .Bytes = jobBytes, .Existing = discard, .Deferred = true}),
                error) ||
      !Uploaded(Bound().Grow(
                    SubjectResidency::Stream::ClusterSlot,
                    {.Usage = kCullUse, .Bytes = jobBytes, .Existing = discard, .Deferred = true}),
                error) ||
      !Uploaded(Bound().Grow(SubjectResidency::Stream::DrawIndex,
                             {.Usage = Native::BufferUse::Index | Native::BufferUse::Storage,
                              .Bytes = base * static_cast<uint32_t>(sizeof(uint32_t)),
                              .Existing = discard,
                              .Deferred = true}),
                error) ||
      !Uploaded(Bound().Grow(SubjectResidency::Stream::DrawArguments,
                             {.Usage = Native::BufferUse::Indirect | Native::BufferUse::Storage,
                              .Bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t)),
                              .Existing = discard,
                              .Deferred = true}),
                error)) {
    Args_.clear();
    Jobs_ = 0;
    return false;
  }
  return HandDrawArguments(true, error);
}

bool SubjectDraw::HandDrawArguments(bool deferred, std::string &error) {
  if (Args_.empty() || !Bound().Buffer(SubjectResidency::Stream::DrawArguments)) { return true; }
  for (size_t at = 0; at * 5u < Args_.size(); ++at) { Args_[at * 5u] = 0u; }
  const auto bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t));
  std::array table = {SubjectResidency::Crossing{.Which = SubjectResidency::Stream::DrawArguments,
                                                 .Usage = Native::BufferUse::Indirect,
                                                 .From = Args_.data(),
                                                 .Bytes = bytes}};
  return Uploaded(Bound().Cross(table, deferred), error);
}

bool SubjectDraw::SetPose(const SubjectPose &pose, std::string &error) {
  ++Reshaped_;
  if (Borrows()) { return true; }
  if (Bound().Shape().Indices == 0) {
    error = "a pose arrived before any mesh, and there is no subject for it to be a pose of";
    return false;
  }
  if (pose.VertexCount != Bound().Shape().Vertices) {
    error = "the pose carries " + std::to_string(pose.VertexCount) +
            " vertices and the subject has " + std::to_string(Bound().Shape().Vertices) +
            ", so it is a different body rather than the same one moved";
    return false;
  }
  if (!pose.Verts.Stands() || !pose.Emitted.Stands()) {
    error = "a pose arrived without positions or emitted radiance, which every draw binds";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = pose.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    ModelBefore_ = ModelStamp_ == 0u ? pose.Model : Model;
    ModelStamp_ = Frame_;
  }
  for (int part = 0; part < 16; part++) { Model[part] = pose.Model[part]; }
  RowsStale_ = true;
  return HandStreams(pose, true, error);
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

}
