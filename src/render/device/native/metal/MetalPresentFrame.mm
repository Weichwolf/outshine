#include "MetalPresentation.h"

namespace outshine::Render::Native::Metal {

MetalPresentFrame::MetalPresentFrame(std::shared_ptr<PresentMailbox> mailbox,
                                     id<CAMetalDrawable> drawable,
                                     PresentIdentity identity)
    : Mailbox_(std::move(mailbox)),
      Drawable_(drawable),
      Image_({.Width = static_cast<uint32_t>(drawable.texture.width),
              .Height = static_cast<uint32_t>(drawable.texture.height),
              .Texels = Format::Bgra8Srgb,
              .Use = TextureUse::ColourAttachment},
             drawable.texture,
             identity.Device),
      Identity_(identity) {}

MetalPresentFrame::~MetalPresentFrame() {
  Release();
}

void MetalPresentFrame::Release() {
  const std::scoped_lock lock(Mailbox_->Mutex);
  if (Mailbox_->ActiveLease != Identity_.Lease) { return; }
  Mailbox_->ActiveLease = 0;
  if (!Mailbox_->Stopped && Mailbox_->Width > 0 && Mailbox_->Height > 0 &&
      Mailbox_->Request == PresentRequest::Idle) {
    Mailbox_->Request = PresentRequest::Requested;
    Mailbox_->Changed.notify_one();
  }
}

bool MetalPresentFrame::Current(uint64_t device) const {
  const std::scoped_lock lock(Mailbox_->Mutex);
  return Identity_.Device == device && !Mailbox_->Stopped &&
         Mailbox_->Generation == Identity_.Generation && Mailbox_->ActiveLease == Identity_.Lease;
}

void MetalPresentFrame::Schedule(id<MTLCommandBuffer> commands) {
  const id<CAMetalDrawable> drawable = Drawable_;
  const dispatch_queue_t presenter = Mailbox_->Presenter;
  [commands addScheduledHandler:^(id<MTLCommandBuffer>) {
    dispatch_async(presenter, ^{
      @autoreleasepool {
        [drawable present];
      }
    });
  }];
  Release();
}

Result<void> MetalCommands::Present(std::unique_ptr<PresentFrame> frame) {
  if (!OnOwner() || Render || Presentation) {
    return Failure(ErrorKind::Invalid,
                   "Metal present requires its owner, closed passes and one frame");
  }
  const auto *const native = dynamic_cast<MetalPresentFrame *>(frame.get());
  if (native == nullptr || !native->Current(Device)) {
    return Failure(ErrorKind::Invalid, "invalid or stale Metal presentation frame");
  }
  const auto *const image = dynamic_cast<const MetalTexture *>(&native->Image());
  const std::array images{image->Recording};
  auto reserved = HoldImages(images);
  if (!reserved) { return reserved; }
  Presentation = std::move(frame);
  return {};
}

}
