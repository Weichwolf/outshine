#include "PieceStore.h"

#include <algorithm>
#include <array>
#include <cstring>

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

namespace {

constexpr uint32_t kPositionFloats = 3;
constexpr uint32_t kNormalFloats = 3;
constexpr uint32_t kUvFloats = 2;
constexpr uint32_t kColourFloats = 4;
constexpr uint32_t kRowFloats = 32;
constexpr uint32_t kSphereFloats = 12;
constexpr uint32_t kArgWords = 5;
constexpr uint32_t kBatchWords = 2;

constexpr SDL_GPUBufferUsageFlags kVertexUse = SDL_GPU_BUFFERUSAGE_VERTEX;
constexpr SDL_GPUBufferUsageFlags kIndexUse =
    SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
constexpr SDL_GPUBufferUsageFlags kReadUse = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
constexpr SDL_GPUBufferUsageFlags kScratchUse =
    SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
constexpr SDL_GPUBufferUsageFlags kDrawIndexUse =
    SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
constexpr SDL_GPUBufferUsageFlags kRowsUse =
    SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;

struct Rebased {
  const PieceMesh *Piece = nullptr;
  uint32_t FirstVertex = 0;
};

void WritePositions(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const Rebased *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPositionFloats > floats) { break; }
    into[at++] = one.pos[0];
    into[at++] = one.pos[1];
    into[at++] = one.pos[2];
  }
}

void WriteNormals(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const Rebased *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kNormalFloats > floats) { break; }
    const Vec3f facing = one.norm();
    into[at++] = facing[0];
    into[at++] = facing[1];
    into[at++] = facing[2];
  }
}

void WriteUv(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const Rebased *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kUvFloats > floats) { break; }
    const Vec2f uv = one.uv();
    into[at++] = uv[0];
    into[at++] = uv[1];
  }
}

void WriteIndices(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const Rebased *>(carrying);
  auto *const words = reinterpret_cast<uint32_t *>(into);
  const std::span<const uint32_t> from = held->Piece->Indices;
  for (uint32_t at = 0; at < floats && at < from.size(); ++at) {
    words[at] = from[at] + held->FirstVertex;
  }
}

} // namespace

PieceStore::Range PieceStore::Take(std::vector<Range> &free, uint32_t count, uint32_t &top) {
  for (size_t at = 0; at < free.size(); ++at) {
    if (free[at].Count < count) { continue; }
    const Range taken{.First = free[at].First, .Count = count};
    free[at].First += count;
    free[at].Count -= count;
    if (free[at].Count == 0) { free.erase(free.begin() + static_cast<long>(at)); }
    return taken;
  }
  const Range taken{.First = top, .Count = count};
  top += count;
  return taken;
}

void PieceStore::Give(std::vector<Range> &free, Range back) {
  if (back.Count == 0) { return; }
  const auto after =
      std::ranges::lower_bound(free, back.First, {}, [](const Range &one) { return one.First; });
  const auto at = free.insert(after, back);
  const size_t here = static_cast<size_t>(at - free.begin());
  if (here + 1 < free.size() && free[here].First + free[here].Count == free[here + 1].First) {
    free[here].Count += free[here + 1].Count;
    free.erase(free.begin() + static_cast<long>(here) + 1);
  }
  if (here > 0 && free[here - 1].First + free[here - 1].Count == free[here].First) {
    free[here - 1].Count += free[here].Count;
    free.erase(free.begin() + static_cast<long>(here));
  }
}

bool PieceStore::Room(std::string &error) {
  const auto bytes = [](uint32_t count, uint32_t wide) {
    return count * wide * static_cast<uint32_t>(sizeof(float));
  };
  return Res_.Grow(
             SubjectResidency::Stream::Vertex, bytes(TopV_, kPositionFloats), kVertexUse, error) &&
         Res_.Grow(
             SubjectResidency::Stream::Normal, bytes(TopV_, kNormalFloats), kVertexUse, error) &&
         Res_.Grow(SubjectResidency::Stream::Uv, bytes(TopV_, kUvFloats), kVertexUse, error) &&
         Res_.Grow(
             SubjectResidency::Stream::Colour, bytes(TopV_, kColourFloats), kVertexUse, error) &&
         Res_.Grow(SubjectResidency::Stream::Index,
                   TopI_ * static_cast<uint32_t>(sizeof(uint32_t)),
                   kIndexUse,
                   error);
}

PieceId PieceStore::Place(const PieceMesh &piece, std::string &error) {
  if (piece.Verts.empty() || piece.Indices.size() < 3 || piece.Indices.size() % 3 != 0) {
    error = "a piece of " + std::to_string(piece.Verts.size()) + " vertices and " +
            std::to_string(piece.Indices.size()) +
            " indices is not a mesh, and placing nothing is not what was asked for";
    return kNoPiece;
  }
  if (!piece.Colours.empty() && piece.Colours.size() != piece.Verts.size() * kColourFloats) {
    error = "a piece carries " + std::to_string(piece.Colours.size() / kColourFloats) +
            " colours over " + std::to_string(piece.Verts.size()) + " vertices";
    return kNoPiece;
  }
  const auto verts = static_cast<uint32_t>(piece.Verts.size());
  const auto indices = static_cast<uint32_t>(piece.Indices.size());
  const Range v = Take(FreeV_, verts, TopV_);
  const Range i = Take(FreeI_, indices, TopI_);
  if (!Room(error)) {
    Give(FreeV_, v);
    Give(FreeI_, i);
    return kNoPiece;
  }

  const Rebased carrying{.Piece = &piece, .FirstVertex = v.First};
  const auto floatsAt = [&v](uint32_t wide) {
    return v.First * wide * static_cast<uint32_t>(sizeof(float));
  };
  std::array<SubjectResidency::Crossing, 5> crossings = {{
      {.Which = SubjectResidency::Stream::Vertex,
       .Usage = kVertexUse,
       .Bytes = verts * kPositionFloats * static_cast<uint32_t>(sizeof(float)),
       .Offset = floatsAt(kPositionFloats),
       .Writes = WritePositions,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Normal,
       .Usage = kVertexUse,
       .Bytes = verts * kNormalFloats * static_cast<uint32_t>(sizeof(float)),
       .Offset = floatsAt(kNormalFloats),
       .Writes = WriteNormals,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Index,
       .Usage = kIndexUse,
       .Bytes = indices * static_cast<uint32_t>(sizeof(uint32_t)),
       .Offset = i.First * static_cast<uint32_t>(sizeof(uint32_t)),
       .Writes = WriteIndices,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Uv,
       .Usage = kVertexUse,
       .Bytes = verts * kUvFloats * static_cast<uint32_t>(sizeof(float)),
       .Offset = floatsAt(kUvFloats),
       .Writes = WriteUv,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Colour,
       .Usage = kVertexUse,
       .From = piece.Colours.data(),
       .Bytes = static_cast<uint32_t>(piece.Colours.size() * sizeof(float)),
       .Offset = floatsAt(kColourFloats)},
  }};
  size_t count = 3;
  if (piece.Textured) { crossings[count++] = crossings[3]; }
  if (!piece.Colours.empty()) { crossings[count++] = crossings[4]; }
  if (!Res_.Cross(std::span<SubjectResidency::Crossing>(crossings.data(), count), false, error)) {
    Give(FreeV_, v);
    Give(FreeI_, i);
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
  held.MaterialSlot = piece.MaterialSlot;
  held.Row = piece.Row;
  held.Clusters.assign(piece.Clusters.begin(), piece.Clusters.end());
  VertexRunsCarried carried;
  carried.Normal = true;
  carried.Uv = piece.Textured;
  carried.Colour = !piece.Colours.empty();
  (void)LayoutOf(carried, held.Layout);
  held.Live = true;
  ++Live_;
  TrianglesLive_ += indices / 3u;
  Dirty_ = true;
  return id;
}

void PieceStore::Release(PieceId which) {
  if (which >= Pieces_.size() || !Pieces_[which].Live) { return; }
  Piece &held = Pieces_[which];
  Give(FreeV_, held.V);
  Give(FreeI_, held.I);
  TrianglesLive_ -= held.I.Count / 3u;
  held.Live = false;
  held.Clusters.clear();
  Spare_.push_back(which);
  --Live_;
  Dirty_ = true;
}

void PieceStore::Retable() {
  Batches_.clear();
  Layouts_.clear();
  Rows_.clear();
  Spheres_.clear();
  JobWords_.clear();
  BatchRows_.clear();
  Args_.clear();
  Jobs_ = 0;

  std::vector<uint32_t> order;
  order.reserve(Pieces_.size());
  for (uint32_t at = 0; at < Pieces_.size(); ++at) {
    if (Pieces_[at].Live) { order.push_back(at); }
  }
  std::ranges::sort(order, [this](uint32_t a, uint32_t b) {
    const Piece &left = Pieces_[a];
    const Piece &right = Pieces_[b];
    if (left.MaterialSlot != right.MaterialSlot) { return left.MaterialSlot < right.MaterialSlot; }
    if (left.Layout != right.Layout) { return left.Layout < right.Layout; }
    return a < b;
  });

  static constexpr Mat4 kUnmoved = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  Rows_.assign(static_cast<size_t>(Pieces_.size()) * kRowFloats, 0.0f);
  for (uint32_t at = 0; at < Pieces_.size(); ++at) {
    const Piece &one = Pieces_[at];
    const Mat4 &row = one.Live ? one.Row : kUnmoved;
    for (size_t term = 0; term < 16u; ++term) {
      const auto held = static_cast<float>(row[term]);
      Rows_[static_cast<size_t>(at) * kRowFloats + term] = held;
      Rows_[static_cast<size_t>(at) * kRowFloats + 16u + term] = held;
    }
  }

  bool anyClusters = false;
  for (const uint32_t at : order) {
    const Piece &one = Pieces_[at];
    const auto row = static_cast<uint32_t>(Batches_.size());
    DrawBatch batch{};
    batch.FirstIndex = one.I.First;
    batch.IndexCount = one.I.Count;
    batch.MaterialSlot = one.MaterialSlot;
    batch.Layout = one.Layout;
    batch.Kind = SurfaceKind::Opaque;
    batch.Draws = 1;
    batch.ModelSlot = at;
    batch.Instances = 1;
    batch.FirstJob = Jobs_;
    for (const DagCluster &cluster : one.Clusters) {
      const auto sphere = static_cast<uint32_t>(Spheres_.size() / kSphereFloats);
      Spheres_.insert(Spheres_.end(),
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
      JobWords_.insert(JobWords_.end(), {sphere, row, one.I.First + cluster.First, cluster.Count});
      ++Jobs_;
    }
    batch.JobCount = Jobs_ - batch.FirstJob;
    anyClusters = anyClusters || batch.JobCount > 0;
    Batches_.push_back(batch);
    Layouts_.push_back(one.Layout);
  }

  if (!anyClusters) {
    Jobs_ = 0;
    JobWords_.clear();
    Spheres_.clear();
    return;
  }
  Args_.assign(Batches_.size() * kArgWords, 0u);
  BatchRows_.assign(Batches_.size() * kBatchWords, 0u);
  uint32_t base = 0;
  for (size_t at = 0; at < Batches_.size(); ++at) {
    const DrawBatch &batch = Batches_[at];
    Args_[at * kArgWords + 1u] = batch.Instances;
    Args_[at * kArgWords + 2u] = base;
    Args_[at * kArgWords + 4u] = batch.ModelSlot;
    BatchRows_[at * kBatchWords] = batch.FirstJob;
    BatchRows_[at * kBatchWords + 1u] = batch.JobCount;
    for (uint32_t one = 0; one < batch.JobCount; ++one) {
      base += JobWords_[(static_cast<size_t>(batch.FirstJob) + one) * DrawList::kJobWords + 3u];
    }
  }
}

bool PieceStore::Hand(std::string &error) {
  if (!Dirty_) { return true; }
  Retable();
  Dirty_ = false;
  if (Batches_.empty()) { return true; }
  const auto bytesOf = [](const auto &held) {
    return static_cast<uint32_t>(held.size() * sizeof(held[0]));
  };
  std::array<SubjectResidency::Crossing, 4> tables = {{
      {.Which = SubjectResidency::Stream::Placements,
       .Usage = kRowsUse,
       .From = Rows_.data(),
       .Bytes = bytesOf(Rows_)},
      {.Which = SubjectResidency::Stream::ClusterSpheres,
       .Usage = kReadUse,
       .From = Spheres_.data(),
       .Bytes = bytesOf(Spheres_)},
      {.Which = SubjectResidency::Stream::ClusterJobs,
       .Usage = kReadUse,
       .From = JobWords_.data(),
       .Bytes = bytesOf(JobWords_)},
      {.Which = SubjectResidency::Stream::ClusterBatches,
       .Usage = kReadUse,
       .From = BatchRows_.data(),
       .Bytes = bytesOf(BatchRows_)},
  }};
  const size_t count = Args_.empty() ? 1u : tables.size();
  if (!Res_.Cross(std::span<SubjectResidency::Crossing>(tables.data(), count), false, error)) {
    return false;
  }
  if (Args_.empty()) { return true; }
  uint32_t base = 0;
  for (uint32_t one = 0; one < Jobs_; ++one) {
    base += JobWords_[static_cast<size_t>(one) * DrawList::kJobWords + 3u];
  }
  const auto words = static_cast<uint32_t>(sizeof(uint32_t));
  return Res_.Grow(SubjectResidency::Stream::ClusterKept, Jobs_ * words, kScratchUse, error) &&
         Res_.Grow(SubjectResidency::Stream::ClusterSlot, Jobs_ * words, kScratchUse, error) &&
         Res_.Grow(SubjectResidency::Stream::DrawIndex, base * words, kDrawIndexUse, error) &&
         HandDrawArguments(false, error);
}

bool PieceStore::HandDrawArguments(bool deferred, std::string &error) {
  if (Args_.empty()) { return true; }
  for (size_t at = 0; at * kArgWords < Args_.size(); ++at) { Args_[at * kArgWords] = 0u; }
  std::array table = {SubjectResidency::Crossing{
      .Which = SubjectResidency::Stream::DrawArguments,
      .Usage = SDL_GPU_BUFFERUSAGE_INDIRECT | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
      .From = Args_.data(),
      .Bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t))}};
  return Res_.Cross(table, deferred, error);
}

} // namespace outshine::Render
