#ifndef OUTSHINE_TEST_RENDER_NATIVETARGETTEXTURES_H
#define OUTSHINE_TEST_RENDER_NATIVETARGETTEXTURES_H

#include "NativeCommandsProxy.h"
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Test::Gpu {
namespace TargetNative = Render::Native;

class TargetTexture final : public TargetNative::Texture {
public:
  TargetTexture(std::unique_ptr<TargetNative::Texture> texture, unsigned &released)
      : Texture(texture->Description()), Texture_(std::move(texture)), Released_(released) {}

  ~TargetTexture() override { ++Released_; }

  [[nodiscard]] TargetNative::Texture &Backing() const { return *Texture_; }

private:
  std::unique_ptr<TargetNative::Texture> Texture_;
  unsigned &Released_;
};

inline TargetNative::Texture &TargetBacking(TargetNative::Texture &texture) {
  const auto *wrapped = dynamic_cast<const TargetTexture *>(&texture);
  return wrapped != nullptr ? wrapped->Backing() : texture;
}

inline const TargetNative::Texture *TargetBacking(const TargetNative::Texture *texture) {
  const auto *wrapped = dynamic_cast<const TargetTexture *>(texture);
  return wrapped != nullptr ? &wrapped->Backing() : texture;
}

inline void TargetBacking(TargetNative::TextureView &view) {
  view.Image = TargetBacking(view.Image);
}

inline std::vector<TargetNative::Binding>
TargetBindings(std::span<const TargetNative::Binding> bindings) {
  std::vector<TargetNative::Binding> result(bindings.begin(), bindings.end());
  for (auto &binding : result) {
    if (auto *sampled = std::get_if<TargetNative::SampledTexture>(&binding.Value)) {
      TargetBacking(sampled->Image);
    } else if (auto *storage = std::get_if<TargetNative::StorageTexture>(&binding.Value)) {
      TargetBacking(storage->Image);
    }
  }
  return result;
}

class TargetTextureCommands final : public NativeCommandsProxy {
public:
  using NativeCommandsProxy::Copy;
  using NativeCommandsProxy::NativeCommandsProxy;

  [[nodiscard]] TargetNative::Result<void> Copy(const TargetNative::Buffer &source,
                                                TargetNative::Texture &destination,
                                                TargetNative::TextureTransfer region) override {
    return NativeCommandsProxy::Copy(source, TargetBacking(destination), region);
  }

  [[nodiscard]] TargetNative::Result<void> Copy(const TargetNative::Texture &source,
                                                TargetNative::Buffer &destination,
                                                TargetNative::TextureTransfer region) override {
    return NativeCommandsProxy::Copy(*TargetBacking(&source), destination, region);
  }

  [[nodiscard]] TargetNative::Result<void>
  Copy(const TargetNative::Texture &source,
       TargetNative::TextureRegion sourceRegion,
       TargetNative::Texture &destination,
       TargetNative::TextureRegion destinationRegion) override {
    return NativeCommandsProxy::Copy(
        *TargetBacking(&source), sourceRegion, TargetBacking(destination), destinationRegion);
  }

  [[nodiscard]] TargetNative::Result<void> Dispatch(const TargetNative::ComputePipeline &pipeline,
                                                    std::span<const TargetNative::Binding> bindings,
                                                    TargetNative::Workgroups groups) override {
    const auto resolved = TargetBindings(bindings);
    return NativeCommandsProxy::Dispatch(pipeline, resolved, groups);
  }

  [[nodiscard]] TargetNative::Result<void>
  BeginRenderPass(const TargetNative::RenderPassDescription &description) override {
    auto resolved = description;
    std::vector<TargetNative::ColourAttachment> colours(description.Colours.begin(),
                                                        description.Colours.end());
    for (auto &colour : colours) { TargetBacking(colour.Image); }
    resolved.Colours = colours;
    if (resolved.Depth) { TargetBacking(resolved.Depth->Image); }
    return NativeCommandsProxy::BeginRenderPass(resolved);
  }

  [[nodiscard]] TargetNative::Result<void>
  Draw(const TargetNative::DrawDescription &description) override {
    auto resolved = description;
    const auto bindings = TargetBindings(description.State.Bindings);
    resolved.State.Bindings = bindings;
    return NativeCommandsProxy::Draw(resolved);
  }

  [[nodiscard]] TargetNative::Result<void>
  DrawIndexed(const TargetNative::IndexedDrawDescription &description) override {
    auto resolved = description;
    const auto bindings = TargetBindings(description.State.Bindings);
    resolved.State.Bindings = bindings;
    return NativeCommandsProxy::DrawIndexed(resolved);
  }

  [[nodiscard]] TargetNative::Result<void>
  DrawIndexedIndirect(const TargetNative::IndirectDrawDescription &description) override {
    auto resolved = description;
    const auto bindings = TargetBindings(description.State.Bindings);
    resolved.State.Bindings = bindings;
    return NativeCommandsProxy::DrawIndexedIndirect(resolved);
  }
};
}
#endif
