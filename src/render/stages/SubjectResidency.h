#ifndef OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H
#define OUTSHINE_RENDER_STAGES_SUBJECTRESIDENCY_H

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
  // WHAT THE RESIDENCY DID PER REBUILD, since the last read. 89 per cent of the hand-over is spent
  // here and a duration alone cannot say whether the answer is fewer calls, a persistent staging
  // buffer, or a layout that needs no copy.
  [[nodiscard]] static size_t UploadsTaken();
  [[nodiscard]] static size_t UploadMBTaken();
  [[nodiscard]] static size_t BuffersMadeTaken();
  [[nodiscard]] static size_t StagingMadeTaken();

  enum class Stream : uint8_t {
    Vertex, Emitted, Normal, Tangent, Uv, Uv1, Colour, Previous, BvhNodes, BvhTriangles,
    Placements, Count
  };

  struct Crossing {
    OwnedBuffer *Into = nullptr;
    uint32_t *Held = nullptr;
    SDL_GPUBufferUsageFlags Usage = 0;
    const void *From = nullptr;
    uint32_t Bytes = 0;

    // A STREAM THAT WRITES ITSELF INTO THE MAPPING RATHER THAN BEING COPIED INTO IT. `From` says
    // "these bytes are over there"; `Writes` says "the memory is yours to fill". SDL hands back a
    // mapped transfer buffer either way, and a producer that already holds float in the device's
    // layout has no reason to assemble a second copy first.
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
  OwnedBuffer BvhNodes, BvhTris;
  OwnedBuffer Placed;
  std::array<uint32_t, (size_t)Stream::Count> Held{};
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

  [[nodiscard]] OwnedBuffer Fill(SDL_GPUBufferUsageFlags usage, const void *from, uint32_t bytes);
  [[nodiscard]] bool Cross(Crossing *what, size_t count, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(Crossing *what, size_t count, uint32_t total, std::string &error);
  [[nodiscard]] bool OpenStaging(uint32_t bytes, std::string &error);
  void FlushCrossings(SDL_GPUCommandBuffer *commands);
  void DropStaged() {
    StagedCount_ = 0;
    StagingUsed_ = 0;
  }
  [[nodiscard]] BoundImage Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind);

private:
  // NO GUESSED DEPTH AND NO GUESSED WIDTH. Both bounds here were numbers this file chose: a ring of
  // THREE transfer buffers rotated per frame, and room for THIRTY-TWO staged runs. The first is
  // what `SDL_MapGPUTransferBuffer`'s `cycle` flag already decides -- the driver knows whether the
  // GPU has finished reading the last contents and renames the buffer when it has not, where a
  // depth of three only hopes. The second refused a frame outright: measured, Khronos's
  // AnimatedCube staged one full pose of 4304 bytes and then a 128-byte hand of placements, and
  // the residency answered "a second full hand in one frame is more than the ring holds" -- over
  // 128 bytes. A run that does not fit now takes a FRESH buffer, and every staged run already
  // records which buffer it came from, so nothing had to be invented to allow it.
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

}

#endif
