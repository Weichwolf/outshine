#ifndef OUTSHINE_RENDER_STAGES_TONEMAPSTAGE_H
#define OUTSHINE_RENDER_STAGES_TONEMAPSTAGE_H

#include <memory>

#include "Extent.h"
#include "math/Vec2.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "Resolve.h"

namespace outshine::Render {

class TonemapStage {
public:
  struct Feeds {
    Native::Texture *Scene = nullptr;
    Native::Texture *Depth = nullptr;
    Native::Sampler *Exact = nullptr;
    Native::Format Linear = Native::Format::Count;
  };

  [[nodiscard]] Native::Result<void>
  Configure(const Gpu &gpu, const Feeds &from, const DisplayOptions &options);

  void Bind(Native::Texture *scene) { Scene = scene; }

  struct Temporal {
    Native::Texture *History = nullptr;
    Native::Texture *Velocity = nullptr;
  };

  void BindTemporal(Temporal from, Extent frame, const Vec2f &jitterDelta, bool historyHeld) {
    const int width = frame.WidthPx;
    const int height = frame.HeightPx;
    History = from.History;
    Velocity = from.Velocity;
    Width = width;
    Height = height;
    JitterDelta[0] = jitterDelta[0];
    JitterDelta[1] = jitterDelta[1];
    HistoryHeld = historyHeld;
  }

  [[nodiscard]] Native::Result<void> Encode(const FrameContext &ctx, const PassRecording &into);

private:
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  DisplayOptions Display_;
  Native::Texture *Scene = nullptr;
  Native::Texture *Depth = nullptr;
  Native::Texture *History = nullptr;
  Native::Texture *Velocity = nullptr;
  bool Temporal = false;
  bool HistoryHeld = false;
  int Width = 0, Height = 0;
  Vec2f JitterDelta = {{0.0f, 0.0f}};
  Native::Sampler *Exact = nullptr;
};

}
#endif
