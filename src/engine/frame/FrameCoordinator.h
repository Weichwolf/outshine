#ifndef OUTSHINE_ENGINE_FRAME_FRAMECOORDINATOR_H
#define OUTSHINE_ENGINE_FRAME_FRAMECOORDINATOR_H

#include "Engine.h"

namespace outshine {

class FrameCoordinator {
public:
  explicit FrameCoordinator(Engine::State &state) noexcept : State_(state) {}

  FrameCoordinator(const FrameCoordinator &) = delete;
  FrameCoordinator &operator=(const FrameCoordinator &) = delete;

  [[nodiscard]] Result Begin();
  [[nodiscard]] Result Render(Extent frame);
  [[nodiscard]] Result End();
  [[nodiscard]] Result Flush();

  [[nodiscard]] bool Open() const noexcept { return Phase_ != Phase::Closed; }

private:
  enum class Phase { Closed, Open, DrawSucceeded };

  Engine::State &State_;
  Phase Phase_ = Phase::Closed;
};

}

#endif
