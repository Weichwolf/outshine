#include "MetalPresentation.h"
#include <SDL3/SDL.h>

namespace outshine::Render::Native::Metal {

MetalPresentation::MetalPresentation(SDL_Window *window,
                                     SDL_MetalView view,
                                     CAMetalLayer *layer,
                                     uint64_t device)
    : Window_(window), View_(view), Device_(device), Mailbox_(std::make_shared<PresentMailbox>()) {
  Mailbox_->Layer = layer;
  const auto priority =
      dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0);
  Mailbox_->Presenter = dispatch_queue_create("outshine.metal.present", priority);
  Worker_ = std::thread([mailbox = Mailbox_] { Run(mailbox); });
}

MetalPresentation::~MetalPresentation() {
  {
    const std::scoped_lock lock(Mailbox_->Mutex);
    Mailbox_->Stopped = true;
    Mailbox_->Ready = nil;
  }
  Mailbox_->Changed.notify_one();
  Worker_.join();
  SDL_Metal_DestroyView(View_);
}

Result<std::unique_ptr<MetalPresentation>>
MetalPresentation::Create(SDL_Window *window, id<MTLDevice> device, uint64_t identity) {
  if (!SDL_IsMainThread()) {
    return Failure(ErrorKind::Invalid, "Metal window attachment requires the main thread");
  }
  auto *const view = SDL_Metal_CreateView(window);
  if (view == nullptr) { return Failure(ErrorKind::Driver, SDL_GetError()); }
  auto *const layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(view);
  if (layer == nil) {
    SDL_Metal_DestroyView(view);
    return Failure(ErrorKind::Driver, "SDL Metal view has no layer");
  }
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = YES;
  layer.allowsNextDrawableTimeout = YES;
  layer.presentsWithTransaction = NO;
  return std::make_unique<MetalPresentation>(window, view, layer, identity);
}

void MetalPresentation::Resize(uint32_t width, uint32_t height) {
  if (Mailbox_->Width == width && Mailbox_->Height == height) { return; }
  ++Mailbox_->Generation;
  Mailbox_->Width = width;
  Mailbox_->Height = height;
  Mailbox_->ActiveLease = 0;
  Mailbox_->Ready = nil;
  Mailbox_->Request = width > 0 && height > 0 ? PresentRequest::Requested : PresentRequest::Idle;
  Mailbox_->Changed.notify_one();
}

Result<std::unique_ptr<PresentFrame>> MetalPresentation::Acquire() {
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(Window_, &width, &height)) {
    return Failure(ErrorKind::Driver, SDL_GetError());
  }
  const auto flags = SDL_GetWindowFlags(Window_);
  const bool suspended = (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0;
  const std::scoped_lock lock(Mailbox_->Mutex);
  Resize(suspended ? 0 : static_cast<uint32_t>(width),
         suspended ? 0 : static_cast<uint32_t>(height));
  if (Mailbox_->ActiveLease != 0) {
    return Failure(ErrorKind::Busy, "Metal presentation already has an acquired frame");
  }
  if (Mailbox_->Request != PresentRequest::Ready) {
    if (Mailbox_->Request == PresentRequest::Idle && Mailbox_->Width > 0 && Mailbox_->Height > 0) {
      Mailbox_->Request = PresentRequest::Requested;
      Mailbox_->Changed.notify_one();
    }
    return std::unique_ptr<PresentFrame>{};
  }
  const auto lease = Mailbox_->NextLease++;
  Mailbox_->ActiveLease = lease;
  auto frame = std::make_unique<MetalPresentFrame>(
      Mailbox_,
      Mailbox_->Ready,
      PresentIdentity{.Device = Device_, .Generation = Mailbox_->Generation, .Lease = lease});
  Mailbox_->Ready = nil;
  Mailbox_->Request = PresentRequest::Idle;
  return frame;
}

void MetalPresentation::Run(const std::shared_ptr<PresentMailbox> &mailbox) {
  std::unique_lock lock(mailbox->Mutex);
  while (!mailbox->Stopped) {
    mailbox->Changed.wait(
        lock, [&] { return mailbox->Stopped || mailbox->Request == PresentRequest::Requested; });
    if (mailbox->Stopped) { return; }
    const auto generation = mailbox->Generation;
    const auto width = mailbox->Width;
    const auto height = mailbox->Height;
    mailbox->Request = PresentRequest::Acquiring;
    lock.unlock();
    @autoreleasepool {
      mailbox->Layer.drawableSize = CGSizeMake(width, height);
      const id<CAMetalDrawable> frame = [mailbox->Layer nextDrawable];
      lock.lock();
      if (mailbox->Stopped) { return; }
      if (generation != mailbox->Generation) { continue; }
      if (frame != nil && (frame.texture.width != width || frame.texture.height != height)) {
        mailbox->Request = PresentRequest::Idle;
        continue;
      }
      mailbox->Ready = frame;
      mailbox->Request = frame == nil ? PresentRequest::Idle : PresentRequest::Ready;
    }
  }
}

Result<void> MetalDevice::Attach(SDL_Window *window) {
  auto presentation = MetalPresentation::Create(window, Native_, Identity_);
  if (!presentation) { return std::unexpected(presentation.error()); }
  Presentation_ = std::move(*presentation);
  return {};
}

Result<std::unique_ptr<PresentFrame>> MetalDevice::AcquirePresentFrame() {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  Collect();
  if (Failed_) { return std::unexpected(*Failed_); }
  if (!Presentation_) { return Failure(ErrorKind::Unsupported, "headless Metal device"); }
  return Presentation_->Acquire();
}

}
