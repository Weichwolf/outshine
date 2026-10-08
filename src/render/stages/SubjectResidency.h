#ifndef OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H
#define OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H

#include <span>
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "GpuOwned.h"
#include "SubjectTypes.h"
#include "shade/TexelChain.h"

namespace outshine::Render {

using Core::TexelKind;
using Core::Texels;

struct SubjectResidency {
  [[nodiscard]] size_t TakeUploadAttempts();

  [[nodiscard]] size_t TotalUploadAttempts() const;

  [[nodiscard]] size_t RecordedCrossings() const;
  [[nodiscard]] size_t TakeUploadBytes();
  [[nodiscard]] size_t TakeBufferAllocationAttempts();
  [[nodiscard]] size_t TakeStagingAllocationAttempts();

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

  enum class ExistingContents { Preserve, Discard };

  struct Need {
    SDL_GPUBufferUsageFlags Usage = 0;
    uint32_t Bytes = 0;
    ExistingContents Existing = ExistingContents::Preserve;
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
    SharedTexture Image;
    SharedSampler Sample;
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
    bool HasEmitted = false;
    bool HasPrevious = false;
  };

  static constexpr size_t kStreams = static_cast<size_t>(Stream::Count);

  struct Range {
    uint32_t First = 0;
    uint32_t Count = 0;
  };

  [[nodiscard]] Range TakeVertices(uint32_t count) { return Take(FreeV_, count, TopV_); }

  void GiveVertices(Range back) { Give(FreeV_, back); }

  [[nodiscard]] Range TakeColours(uint32_t count) { return Take(FreeC_, count, TopC_); }

  void GiveColours(Range back) { Give(FreeC_, back); }

  [[nodiscard]] Range TakeTangents(uint32_t count) { return Take(FreeT_, count, TopT_); }

  void GiveTangents(Range back) { Give(FreeT_, back); }

  [[nodiscard]] Range TakeIndices(uint32_t count) { return Take(FreeI_, count, TopI_); }

  void GiveIndices(Range back) { Give(FreeI_, back); }

  [[nodiscard]] uint32_t VertexRoom() const { return TopV_; }

  [[nodiscard]] uint32_t IndexRoom() const { return TopI_; }

  [[nodiscard]] const Range &SubjectVertices() const { return SubjectV_; }

  [[nodiscard]] const Range &SubjectIndices() const { return SubjectI_; }

  [[nodiscard]] const Range &SubjectColours() const { return SubjectC_; }

  [[nodiscard]] const Range &SubjectTangents() const noexcept { return SubjectT_; }

  [[nodiscard]] uint32_t TangentRoom() const noexcept { return TopT_; }

  void SubjectStands(Range vertices, Range indices, Range colours, Range tangents) {
    SubjectV_ = vertices;
    SubjectI_ = indices;
    SubjectC_ = colours;
    SubjectT_ = tangents;
  }

  void StandsOn(SDL_GPUDevice *device) {
    if (Device_ != device) { DefaultImages_.clear(); }
    Device_ = device;
  }

  [[nodiscard]] SDL_GPUDevice *Device() const { return Device_; }

  [[nodiscard]] OwnedBuffer &Buffer(Stream which) { return Buffers_[static_cast<size_t>(which)]; }

  [[nodiscard]] const OwnedBuffer &Buffer(Stream which) const {
    return Buffers_[static_cast<size_t>(which)];
  }

  [[nodiscard]] uint32_t *HeldAt(Stream which) { return &Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t HeldOf(Stream which) const { return Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t StagedBytes() const { return StagedThisFrame_; }

  void ForgetStagedCount() { StagedThisFrame_ = 0; }

  [[nodiscard]] uint64_t HeldBytes() const {
    uint64_t bytes = 0;
    for (const uint32_t one : Held_) { bytes += one; }
    return bytes;
  }

  struct AllocationStats {
    std::array<uint32_t, kStreams> StreamBytes{};
    uint64_t TransferBytes = 0;
    uint32_t VertexSlots = 0;
    uint32_t FreeVertexSlots = 0;
    uint32_t IndexSlots = 0;
    uint32_t FreeIndexSlots = 0;
  };

  [[nodiscard]] AllocationStats Allocations() const;

  [[nodiscard]] Shaping &Shape() { return Shape_; }

  [[nodiscard]] const Shaping &Shape() const { return Shape_; }

  [[nodiscard]] bool Cross(std::span<Crossing> what, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(std::span<Crossing> what, uint32_t total, std::string &error);
  [[nodiscard]] bool Grow(Stream which, Need need, std::string &error);
  [[nodiscard]] bool FlushCrossings(SDL_GPUCommandBuffer *commands, std::string &error);
  void CommitCrossings();

  [[nodiscard]] bool SubmitPendingUploads(std::string &error);

  [[nodiscard]] std::expected<BoundImage, std::string>
  Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const;

private:
  [[nodiscard]] std::expected<void, std::string>
  UploadMipChain(OwnedTexture &image, ImageView pixels, uint32_t levels, ImageMipKind kind) const;

  [[nodiscard]] std::expected<BoundImage, std::string>
  UploadImage(const SubjectTexture &texture, Transfer decode, TexelKind kind) const;

  struct DefaultImageKey {
    Transfer Decode;
    TexelKind Kind;
    SubjectWrap WrapU, WrapV;
    SubjectFilter Magnify, Minify;
    SubjectMip Mip;

    bool operator==(const DefaultImageKey &) const = default;
  };

  struct DefaultImage {
    DefaultImageKey Key;
    BoundImage Image;
  };

  mutable std::vector<DefaultImage> DefaultImages_;
  mutable size_t UploadAttempts_ = 0;
  mutable size_t TotalUploadAttempts_ = 0;
  size_t RecordedCrossings_ = 0;
  mutable size_t UploadBytes_ = 0;
  size_t BufferAttempts_ = 0;
  mutable size_t StagingAttempts_ = 0;

  struct BufferChanges {
    std::array<OwnedBuffer, kStreams> Buffers;
    std::array<uint32_t, kStreams> Capacities{};
    std::array<bool, kStreams> Changed{};
  };

  [[nodiscard]] bool
  PrepareBuffers(std::span<Crossing> what, BufferChanges &previous, std::string &error);
  void RestoreBuffers(BufferChanges &previous);
  [[nodiscard]] bool StageUploads(std::span<Crossing> what, uint32_t total, std::string &error);
  [[nodiscard]] bool ReplacesBuffers(std::span<const Crossing> crossings) const;
  void RecordCrossings(SDL_GPUCopyPass *copy);
  [[nodiscard]] static Range Take(std::vector<Range> &free, uint32_t count, uint32_t &top);
  static void Give(std::vector<Range> &free, Range back);
  std::vector<Range> FreeV_;
  std::vector<Range> FreeI_;
  std::vector<Range> FreeC_;
  std::vector<Range> FreeT_;
  uint32_t TopV_ = 0;
  uint32_t TopI_ = 0;
  uint32_t TopC_ = 0;
  uint32_t TopT_ = 0;
  Range SubjectV_;
  Range SubjectI_;
  Range SubjectC_;
  Range SubjectT_;

  struct Staged {
    SDL_GPUBuffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    uint32_t Offset = 0;
    SDL_GPUTransferBuffer *Staging = nullptr;
  };

  SDL_GPUDevice *Device_ = nullptr;
  std::array<OwnedBuffer, kStreams> Buffers_;
  std::array<uint32_t, kStreams> Held_{};
  Shaping Shape_;

  OwnedTransfer Staging_;

  struct RetiredTransfer {
    OwnedTransfer Buffer;
    uint32_t Bytes = 0;
  };

  std::vector<RetiredTransfer> Retired_;
  uint32_t StagingBytes_ = 0;
  uint32_t StagingUsed_ = 0;
  uint32_t PendingUploadBytes_ = 0;
  uint32_t StagedThisFrame_ = 0;
  std::vector<Staged> Staged_;
  size_t StagedCount_ = 0;

  OwnedTransfer Bulk_;
  uint32_t BulkBytes_ = 0;
};

}

#endif
