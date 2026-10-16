#ifndef OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H
#define OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H

#include <span>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "native/Device.h"

#include "SubjectTypes.h"
#include "shade/TexelChain.h"

namespace outshine::Render {
class UploadQueue;

using Core::TexelKind;
using Core::Texels;

struct SubjectResidency {
  void UploadsFrom(UploadQueue *queue) noexcept { Uploads_ = queue; }

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
    Native::UseMask<Native::BufferUse> Usage;
    uint32_t Bytes = 0;
    ExistingContents Existing = ExistingContents::Preserve;
    bool Deferred = false;
  };

  struct Crossing {
    Stream Which = Stream::Count;
    Native::UseMask<Native::BufferUse> Usage;
    const void *From = nullptr;
    uint32_t Bytes = 0;
    uint32_t Offset = 0;

    void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
    const void *Carrying = nullptr;

    [[nodiscard]] bool Stands() const { return From != nullptr || Writes != nullptr; }
  };

  struct BoundImage {
    std::shared_ptr<Native::Texture> Image;
    std::shared_ptr<Native::Sampler> Sample;
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

  [[nodiscard]] Native::Result<void> StandsOn(Native::Device *device);

  [[nodiscard]] Native::Device *Device() const { return Device_; }

  [[nodiscard]] std::unique_ptr<Native::Buffer> &Buffer(Stream which) {
    return Buffers_[static_cast<size_t>(which)];
  }

  [[nodiscard]] const std::unique_ptr<Native::Buffer> &Buffer(Stream which) const {
    return Buffers_[static_cast<size_t>(which)];
  }

  [[nodiscard]] uint32_t *HeldAt(Stream which) { return &Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t HeldOf(Stream which) const { return Held_[static_cast<size_t>(which)]; }

  [[nodiscard]] uint32_t StagedBytes() const { return StagedThisFrame_; }

  void ForgetStagedCount() { StagedThisFrame_ = 0; }

  [[nodiscard]] uint64_t HeldBytes() const {
    uint64_t bytes = 0;
    for (const uint32_t one : Held_) { bytes += one; }
    for (const auto &one : RetiredBuffers_) { bytes += one->Description().Bytes; }
    return bytes;
  }

  struct AllocationStats {
    std::array<uint32_t, kStreams> StreamBytes{};
    uint64_t TransferBytes = 0;
    uint64_t RetiredDeviceBytes = 0;
    uint64_t DeviceCopyBytes = 0;
    uint32_t VertexSlots = 0;
    uint32_t FreeVertexSlots = 0;
    uint32_t IndexSlots = 0;
    uint32_t FreeIndexSlots = 0;
  };

  [[nodiscard]] AllocationStats Allocations() const;

  [[nodiscard]] Shaping &Shape() { return Shape_; }

  [[nodiscard]] const Shaping &Shape() const { return Shape_; }

  [[nodiscard]] Native::Result<void> Cross(std::span<Crossing> what, bool deferred);
  [[nodiscard]] Native::Result<void> Grow(Stream which, Need need);
  [[nodiscard]] Native::Result<void> FlushCrossings(Native::Commands *commands);
  void CommitCrossings();
  void ReleaseUploadScratch();

  [[nodiscard]] Native::Result<void> SubmitPendingUploads();

  [[nodiscard]] Native::Result<BoundImage>
  Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const;

private:
  [[nodiscard]] Native::Result<void> Submit(std::span<Crossing> what, uint32_t total);
  [[nodiscard]] Native::Result<void> UploadMipChain(std::unique_ptr<Native::Texture> &image,
                                                    ImageView pixels,
                                                    uint32_t levels,
                                                    ImageMipKind kind) const;

  [[nodiscard]] Native::Result<BoundImage>
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
    std::array<std::unique_ptr<Native::Buffer>, kStreams> Buffers;
    std::array<uint32_t, kStreams> Capacities{};
    std::array<bool, kStreams> Changed{};
  };

  [[nodiscard]] Native::Result<void> PrepareBuffers(std::span<Crossing> what,
                                                    BufferChanges &previous);
  void RestoreBuffers(BufferChanges &previous);
  void RetireBuffers(BufferChanges &previous);
  [[nodiscard]] Native::Result<void>
  PreserveBuffer(Native::Buffer &source, Native::Buffer &target, uint32_t bytes, bool deferred);
  [[nodiscard]] Native::Result<void> StageUploads(std::span<Crossing> what, uint32_t total);
  [[nodiscard]] Native::Result<void> RecordCrossings(Native::Commands &commands);
  [[nodiscard]] Native::Result<std::span<std::byte>> PrepareStaging(uint32_t total);
  [[nodiscard]] Native::Result<std::span<std::byte>> PrepareBulk(uint32_t total);
  static void Fill(std::span<std::byte> mapped, uint32_t at, std::span<const Crossing> what);
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
    Native::Buffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    uint32_t Offset = 0;
    Native::Buffer *Staging = nullptr;
  };

  UploadQueue *Uploads_ = nullptr;
  Native::Device *Device_ = nullptr;
  std::array<std::unique_ptr<Native::Buffer>, kStreams> Buffers_;
  std::array<uint32_t, kStreams> Held_{};
  Shaping Shape_;

  std::unique_ptr<Native::Buffer> Staging_;

  struct RetiredTransfer {
    std::unique_ptr<Native::Buffer> Buffer;
    uint32_t Bytes = 0;
  };

  std::vector<RetiredTransfer> Retired_;
  std::vector<std::unique_ptr<Native::Buffer>> RetiredBuffers_;
  uint64_t DeviceCopyBytes_ = 0;
  uint32_t StagingBytes_ = 0;
  uint32_t StagingUsed_ = 0;
  uint32_t PendingUploadBytes_ = 0;
  uint32_t StagedThisFrame_ = 0;
  std::vector<Staged> Staged_;
  size_t StagedCount_ = 0;

  std::unique_ptr<Native::Buffer> Bulk_;
  uint32_t BulkBytes_ = 0;
};

}

#endif
