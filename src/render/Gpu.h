/* THE DEVICE-LEVEL HANDLES A STAGE GETS, handed once at Init and never per frame. Renderer owns the
 * real device and every pass boundary; a stage never re-derives or requests them itself. */
#ifndef GPU_H
#define GPU_H

#include <SDL3/SDL_gpu.h>

#include "RenderCatalogue.h"

namespace outshine::Render {

struct Gpu {
  SDL_GPUDevice *Device = nullptr;
  SDL_GPUTextureFormat HdrFormat = SDL_GPU_TEXTUREFORMAT_INVALID;   /* the offscreen scene target */
  SDL_GPUTextureFormat SurfaceFormat = SDL_GPU_TEXTUREFORMAT_INVALID; /* the frame a picture is read off */
  int Width = 0, Height = 0;   /* the declared scene resolution */
  /* Whether the device can filter a 32-bit float texture. A stage that wants an EXACT texel and a
   * filter over exact texels needs it: without it such a texture may only be sampled unfiltered. */
  bool FiltersFloat32 = false;
  /* THE COLOUR TARGETS THE SCENE PASS ATTACHES, in attachment order, as the COMPILED PLAN resolved
   * them (board:1121). A geometry stage builds its pipelines and chooses its fragment outputs from
   * this and never from a count of its own: the plan prunes a target no held stage reads, and a
   * pipeline that still declared it renders correctly and is undefined -- Metal aborts on
   * "for color attachment 1, the renderPipelineState pixelFormat must be MTLPixelFormatInvalid, as
   * no texture is set". The set is the one place the two can agree. */
  AttachmentSet SceneColours;
};

/* WHERE A STAGE RECORDS, and it is two handles rather than one because SDL_GPU splits them: draws go
 * to the pass, and uniform data is pushed on the command buffer that opened it. A stage that held
 * only the pass would have no way to hand its shader a number. */
struct PassRecording {
  SDL_GPUCommandBuffer *Commands = nullptr;
  SDL_GPURenderPass *Pass = nullptr;
};

} // namespace outshine::Render
#endif
