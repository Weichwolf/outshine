#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALPRESENTATION_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALPRESENTATION_H

#include "MetalInternal.h"
#include <SDL3/SDL_metal.h>
#include <dispatch/dispatch.h>
#import <QuartzCore/CAMetalLayer.h>

namespace outshine::Render::Native::Metal {

enum class PresentRequest { Idle, Requested, Acquiring, Ready };

struct PresentMailbox {
  std::mutex Mutex;
  std::condition_variable Changed;
  CAMetalLayer *Layer;
  dispatch_queue_t Presenter;
  id<CAMetalDrawable> Ready = nil;
  PresentRequest Request = PresentRequest::Idle;
  uint64_t Generation = 0, NextLease = 1, ActiveLease = 0;
  uint32_t Width = 0, Height = 0;
  bool Stopped = false;
};

struct PresentIdentity {
  uint64_t Device, Generation, Lease;
};

class MetalPresentFrame final : public PresentFrame {
public:
  MetalPresentFrame(std::shared_ptr<PresentMailbox> mailbox,
                    id<CAMetalDrawable> drawable,
                    PresentIdentity identity);
  ~MetalPresentFrame() override;

  [[nodiscard]] const Texture &Image() const noexcept override { return Image_; }

  [[nodiscard]] bool Current(uint64_t device) const;
  void Schedule(id<MTLCommandBuffer> commands);

private:
  void Release();

  std::shared_ptr<PresentMailbox> Mailbox_;
  id<CAMetalDrawable> Drawable_;
  MetalTexture Image_;
  PresentIdentity Identity_;
};

class MetalPresentation {
public:
  [[nodiscard]] static Result<std::unique_ptr<MetalPresentation>>
  Create(SDL_Window *window, id<MTLDevice> device, uint64_t identity);
  MetalPresentation(SDL_Window *window, SDL_MetalView view, CAMetalLayer *layer, uint64_t device);
  ~MetalPresentation();
  MetalPresentation(const MetalPresentation &) = delete;
  MetalPresentation &operator=(const MetalPresentation &) = delete;

  [[nodiscard]] Result<std::unique_ptr<PresentFrame>> Acquire();

  [[nodiscard]] bool Targets(SDL_Window *window) const noexcept { return Window_ == window; }

private:
  void Resize(uint32_t width, uint32_t height);
  static void Run(const std::shared_ptr<PresentMailbox> &mailbox);

  SDL_Window *Window_;
  SDL_MetalView View_;
  uint64_t Device_;
  std::shared_ptr<PresentMailbox> Mailbox_;
  std::thread Worker_;
};

}
#endif
