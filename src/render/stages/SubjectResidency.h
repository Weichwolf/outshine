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
    OwnedBuffer *Into = nullptr;
    uint32_t *Held = nullptr;
    SDL_GPUBufferUsageFlags Usage = 0;
    const void *From = nullptr;
    uint32_t Bytes = 0;

    void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
    const void *Carrying = nullptr;

    [[nodiscard]] bool Stands() const { return From != nullptr || Writes != nullptr; }
  };

  struct BoundImage {
    OwnedTexture Image;
    OwnedSampler Sample;
  };

  enum class Transfer { Srgb, Linear };

  SDL_GPUDevice *Device = nullptr;
  bool FiltersFloat32 = false;

  OwnedBuffer Vtx, Uv, Uv1, Nrm, Tan, Col, Emit, Idx, Prev;
  OwnedBuffer ClusterSpheres, ClusterJobs, ClusterBatches;
  OwnedBuffer ClusterKept, ClusterSlot, DrawIdx, DrawArgs;
  OwnedBuffer Placed;
  std::array<uint32_t, static_cast<size_t>(Stream::Count)> Held{};

  [[nodiscard]] uint32_t StagedBytes() const { return StagedThisFrame_; }

  void ForgetStagedCount() { StagedThisFrame_ = 0; }

  [[nodiscard]] uint32_t HeldBytes() const {
    uint32_t bytes = 0;
    for (const uint32_t one : Held) { bytes += one; }
    return bytes;
  }

  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasTangent = false;
  bool HasColour = false;

  [[nodiscard]] bool Cross(std::span<Crossing> what, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(std::span<Crossing> what, uint32_t total, std::string &error);
  void FlushCrossings(SDL_GPUCommandBuffer *commands);

  void DropStaged() {
    StagedCount_ = 0;
    StagingUsed_ = 0;
  }

  [[nodiscard]] BoundImage
  Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const;

private:
  struct Staged {
    SDL_GPUBuffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    SDL_GPUTransferBuffer *Staging = nullptr;
  };

  OwnedTransfer Staging_{};
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
