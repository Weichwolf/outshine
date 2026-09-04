#ifndef OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H
#define OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H

#include <span>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "GpuOwned.h"
#include "SubjectTypes.h"
#include "TexelChain.h"

namespace outshine::Render {

struct SubjectResidency {
  [[nodiscard]] static size_t UploadsTaken();

  [[nodiscard]] static size_t UploadsEver();

  [[nodiscard]] static size_t CrossingsFlushed();
  [[nodiscard]] static size_t UploadMBTaken();
  [[nodiscard]] static size_t BuffersMadeTaken();
  [[nodiscard]] static size_t StagingMadeTaken();

  enum class Stream : uint8_t {
    Vertex,
    Emitted,
    Normal,
    Tangent,
    Uv,
    Uv1,
    Colour,
    Previous,
    Placements,
    Index,
    ClusterSpheres,
    ClusterJobs,
    ClusterBatches,
    ClusterKept,
    ClusterSlot,
    DrawIndex,
    DrawArguments,
    Count
  };

  struct Crossing {
    Stream Which = Stream::Count;
    SDL_GPUBufferUsageFlags Usage = 0;
    const void *From = nullptr;
    uint32_t Bytes = 0;
    uint32_t Offset = 0;

    void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
    const void *Carrying = nullptr;

    [[nodiscard]] bool Stands() const { return From != nullptr || Writes != nullptr; }
  };

  struct BoundImage {
    OwnedTexture Image;
    OwnedSampler Sample;
  };

  enum class Transfer { Srgb, Linear };

  struct Shaping {
    uint32_t Vertices = 0;
    uint32_t Indices = 0;
    bool HasUv = false;
    bool HasUv1 = false;
    bool HasNormal = false;
    bool HasTangent = false;
    bool HasColour = false;
  };

  static constexpr size_t kStreams = static_cast<size_t>(Stream::Count);

  struct Range {
    uint32_t First = 0;
    uint32_t Count = 0;
  };

  [[nodiscard]] Range TakeVertices(uint32_t count) { return Take(FreeV_, count, TopV_); }

  void GiveVertices(Range back) { Give(FreeV_, back); }

  [[nodiscard]] Range TakeIndices(uint32_t count) { return Take(FreeI_, count, TopI_); }

  void GiveIndices(Range back) { Give(FreeI_, back); }

  [[nodiscard]] uint32_t VertexRoom() const { return TopV_; }

  [[nodiscard]] uint32_t IndexRoom() const { return TopI_; }

  [[nodiscard]] const Range &SubjectVertices() const { return SubjectV_; }

  [[nodiscard]] const Range &SubjectIndices() const { return SubjectI_; }

  void SubjectStands(Range vertices, Range indices) {
    SubjectV_ = vertices;
    SubjectI_ = indices;
  }

  void StandsOn(SDL_GPUDevice *device, bool filtersFloat32) {
    Device_ = device;
    FiltersFloat32_ = filtersFloat32;
  }

  [[nodiscard]] SDL_GPUDevice *Device() const { return Device_; }

  [[nodiscard]] bool FiltersFloat32() const { return FiltersFloat32_; }

  [[nodiscard]] OwnedBuffer &Buffer(Stream which) { return Buffers_[static_cast<size_t>(which)]; }

  [[nodiscard]] const OwnedBuffer &Buffer(Stream which) const {
    return Buffers_[static_cast<size_t>(which)];
  }

  [[nodiscard]] uint32_t *HeldAt(Stream which) { return &Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t HeldOf(Stream which) const { return Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t StagedBytes() const { return StagedThisFrame_; }

  void ForgetStagedCount() { StagedThisFrame_ = 0; }

  [[nodiscard]] uint32_t HeldBytes() const {
    uint32_t bytes = 0;
    for (const uint32_t one : Held_) { bytes += one; }
    return bytes;
  }

  [[nodiscard]] Shaping &Shape() { return Shape_; }

  [[nodiscard]] const Shaping &Shape() const { return Shape_; }

  [[nodiscard]] bool Cross(std::span<Crossing> what, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(std::span<Crossing> what, uint32_t total, std::string &error);
  [[nodiscard]] bool
  Grow(Stream which, uint32_t bytes, SDL_GPUBufferUsageFlags usage, std::string &error);
  void FlushCrossings(SDL_GPUCommandBuffer *commands);

  void DropStaged() {
    StagedCount_ = 0;
    StagingUsed_ = 0;
  }

  [[nodiscard]] BoundImage
  Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const;

private:
  [[nodiscard]] static Range Take(std::vector<Range> &free, uint32_t count, uint32_t &top);
  static void Give(std::vector<Range> &free, Range back);
  std::vector<Range> FreeV_;
  std::vector<Range> FreeI_;
  uint32_t TopV_ = 0;
  uint32_t TopI_ = 0;
  Range SubjectV_;
  Range SubjectI_;

  struct Staged {
    SDL_GPUBuffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    uint32_t Offset = 0;
    SDL_GPUTransferBuffer *Staging = nullptr;
  };

  SDL_GPUDevice *Device_ = nullptr;
  bool FiltersFloat32_ = false;
  std::array<OwnedBuffer, kStreams> Buffers_;
  std::array<uint32_t, kStreams> Held_{};
  Shaping Shape_;

  OwnedTransfer Staging_;
  std::vector<OwnedTransfer> Retired_;
  uint32_t StagingBytes_ = 0;
  uint32_t StagingUsed_ = 0;
  uint32_t StagedThisFrame_ = 0;
  std::vector<Staged> Staged_;
  size_t StagedCount_ = 0;

  OwnedTransfer Bulk_;
  uint32_t BulkBytes_ = 0;
};

} // namespace outshine::Render

#endif
