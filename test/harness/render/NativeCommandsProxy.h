#ifndef OUTSHINE_TEST_RENDER_NATIVECOMMANDSPROXY_H
#define OUTSHINE_TEST_RENDER_NATIVECOMMANDSPROXY_H
#include "src/render/device/native/Commands.h"
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Test::Gpu {
class NativeCommandsProxy : public Render::Native::Commands {
public:
  explicit NativeCommandsProxy(std::unique_ptr<Render::Native::Commands> commands)
      : Backing_(std::move(commands)) {}

  [[nodiscard]] std::unique_ptr<Render::Native::Commands> Take() { return std::move(Backing_); }

  [[nodiscard]] Render::Native::Result<void> Copy(const Render::Native::Buffer &source,
                                                  Render::Native::Buffer &destination,
                                                  Render::Native::BufferCopy region) override {
    return Backing_->Copy(source, destination, region);
  }

  [[nodiscard]] Render::Native::Result<void> Copy(const Render::Native::Buffer &source,
                                                  Render::Native::Texture &destination,
                                                  Render::Native::TextureTransfer region) override {
    return Backing_->Copy(source, destination, region);
  }

  [[nodiscard]] Render::Native::Result<void> Copy(const Render::Native::Texture &source,
                                                  Render::Native::Buffer &destination,
                                                  Render::Native::TextureTransfer region) override {
    return Backing_->Copy(source, destination, region);
  }

  [[nodiscard]] Render::Native::Result<void>
  Copy(const Render::Native::Texture &source,
       Render::Native::TextureRegion sourceRegion,
       Render::Native::Texture &destination,
       Render::Native::TextureRegion destinationRegion) override {
    return Backing_->Copy(source, sourceRegion, destination, destinationRegion);
  }

  [[nodiscard]] Render::Native::Result<void>
  Dispatch(const Render::Native::ComputePipeline &pipeline,
           std::span<const Render::Native::Binding> bindings,
           Render::Native::Workgroups groups) override {
    return Backing_->Dispatch(pipeline, bindings, groups);
  }

  [[nodiscard]] Render::Native::Result<void>
  BeginRenderPass(const Render::Native::RenderPassDescription &description) override {
    return Backing_->BeginRenderPass(description);
  }

  [[nodiscard]] Render::Native::Result<void>
  Draw(const Render::Native::DrawDescription &description) override {
    return Backing_->Draw(description);
  }

  [[nodiscard]] Render::Native::Result<void>
  DrawIndexed(const Render::Native::IndexedDrawDescription &description) override {
    return Backing_->DrawIndexed(description);
  }

  [[nodiscard]] Render::Native::Result<void>
  DrawIndexedIndirect(const Render::Native::IndirectDrawDescription &description) override {
    return Backing_->DrawIndexedIndirect(description);
  }

  [[nodiscard]] Render::Native::Result<void> EndRenderPass() override {
    return Backing_->EndRenderPass();
  }

  [[nodiscard]] Render::Native::Result<void>
  Present(std::unique_ptr<Render::Native::PresentFrame> frame) override {
    return Backing_->Present(std::move(frame));
  }

  void BeginLabel(std::string_view name) override { Backing_->BeginLabel(name); }

  void EndLabel() override { Backing_->EndLabel(); }

private:
  std::unique_ptr<Render::Native::Commands> Backing_;
};
}
#endif
