#ifndef OUTSHINE_RENDER_STAGES_SUBJECTLIGHTS_H
#define OUTSHINE_RENDER_STAGES_SUBJECTLIGHTS_H
#include "LightCells.h"
#include "SubjectTypes.h"
#include "native/Device.h"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
class UploadQueue;

class SubjectLights {
public:
  static constexpr uint32_t kEmitterBinding = 24, kIndexBinding = 25, kFrameBinding = 4;

  [[nodiscard]] Native::Result<void> Attach(Native::Device &device, UploadQueue *uploads);
  [[nodiscard]] Native::Result<void> Set(std::span<const SubjectLight> sources);
  [[nodiscard]] std::span<const SubjectLight> Sources() const noexcept;

  [[nodiscard]] bool Spatial() const noexcept { return Sources().size() > kMaxSubjectLights; }

  [[nodiscard]] bool FullEnumeration() const noexcept;
  [[nodiscard]] Native::Buffer *Emitters() const noexcept;
  [[nodiscard]] Native::Buffer *Index() const noexcept;
  [[nodiscard]] uint64_t EmitterBytes() const noexcept;
  [[nodiscard]] uint64_t IndexBytes() const noexcept;

  void Record() noexcept { Recorded_ = true; }

  void Submitted(Native::Completion completion) noexcept;

  void Aborted() noexcept { Recorded_ = false; }

private:
  struct Slot {
    std::vector<SubjectLight> Sources;
    std::optional<LightCells> Cells;
    std::unique_ptr<Native::Buffer> Emitters, Index, Upload;
    std::optional<Native::Completion> LastUse;
  };

  [[nodiscard]] Native::Result<size_t> FreeSlot();
  [[nodiscard]] Native::Result<void> Upload(Slot &slot, const LightCells &cells);
  std::array<Slot, 3> Slots_;
  std::optional<size_t> Current_;
  std::vector<SubjectLight> Pending_;
  Native::Device *Device_ = nullptr;
  UploadQueue *Uploads_ = nullptr;
  bool Recorded_ = false;
};
}
#endif
