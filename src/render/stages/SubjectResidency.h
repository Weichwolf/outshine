#ifndef OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H
#define OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H

#include <array>
#include <cstdint>
#include <string>

#include <SDL3/SDL_gpu.h>

#include "GpuOwned.h"
#include "SubjectTypes.h"
#include "TexelChain.h"

namespace outshine::Render {

struct SubjectResidency {
  enum class Stream : uint8_t {
    Vertex, Emitted, Normal, Tangent, Uv, Uv1, Colour, Previous, BvhNodes, BvhTriangles, Count
  };

  struct Crossing {
    OwnedBuffer *Into = nullptr;
    uint32_t *Held = nullptr;
    SDL_GPUBufferUsageFlags Usage = 0;
    const void *From = nullptr;
    uint32_t Bytes = 0;
  };

  struct BoundImage {
    OwnedTexture Image;
    OwnedSampler Sample;
  };

  enum class Transfer { Srgb, Linear };

  SDL_GPUDevice *Device = nullptr;
  bool FiltersFloat32 = false;

  OwnedBuffer Vtx, Uv, Uv1, Nrm, Tan, Col, Emit, Idx, Prev;
  OwnedBuffer BvhNodes, BvhTris;
  std::array<uint32_t, (size_t)Stream::Count> Held{};
  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasTangent = false;
  bool HasColour = false;

  [[nodiscard]] OwnedBuffer Fill(SDL_GPUBufferUsageFlags usage, const void *from, uint32_t bytes);
  [[nodiscard]] bool Cross(Crossing *what, size_t count, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(Crossing *what, size_t count, uint32_t total, std::string &error);
  // the staging ring, sized ONCE at residency establishment from the mesh's own streams
  [[nodiscard]] bool OpenStaging(uint32_t bytes, std::string &error);
  void FlushCrossings(SDL_GPUCommandBuffer *commands);
  void DropStaged() {
    StagedCount_ = 0;
    StagingUsed_ = 0;
  }
  [[nodiscard]] BoundImage Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind);

private:
  static constexpr size_t kStagingRing = 3;
  // derived: one residency crosses at most 8 vertex streams + 2 BVH runs per frame --
  // ONE full pose, which is also all the ring's BYTES hold (OpenStaging sums the standing
  // streams); 32 entries is that population with three-fold slack so the entry budget can
  // never refuse before the byte budget speaks
  static constexpr size_t kStagedCrossings = 32;
  struct Staged {
    SDL_GPUBuffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    SDL_GPUTransferBuffer *Staging = nullptr;
  };
  std::array<OwnedTransfer, kStagingRing> Staging_{};
  uint32_t StagingBytes_ = 0;
  uint32_t StagingUsed_ = 0;
  size_t StagingAt_ = 0;
  std::array<Staged, kStagedCrossings> Staged_{};
  size_t StagedCount_ = 0;
};

}

#endif
