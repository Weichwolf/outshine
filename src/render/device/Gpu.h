#ifndef OUTSHINE_RENDER_DEVICE_GPU_H
#define OUTSHINE_RENDER_DEVICE_GPU_H

#include "native/Device.h"

#include "RenderCatalogue.h"

namespace outshine::Render {

class UploadQueue;

struct Gpu {
  Native::Device *Device = nullptr;
  Native::Format HdrFormat = Native::Format::Count;
  Native::Format SurfaceFormat = Native::Format::Count;
  int Width = 0, Height = 0;

  AttachmentSet SceneColours;
  UploadQueue *Uploads = nullptr;
};

class StageSubmission;

struct PassRecording {
  Native::Commands *Commands = nullptr;
  StageSubmission &Submission;
  Native::Viewport Area;
  Native::Scissor Clip;
};

}
#endif
