#ifndef OUTSHINE_TEST_RENDER_NATIVEOFFLINEDEVICE_H
#define OUTSHINE_TEST_RENDER_NATIVEOFFLINEDEVICE_H
#include "NativeDeviceProxy.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <print>
#include <string>
#include <utility>

namespace outshine::Test::Gpu {
class NativeOfflineDevice final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;

  [[nodiscard]] Render::Native::Result<Render::Native::Completion>
  Submit(std::unique_ptr<Render::Native::Commands> commands) override {
    auto done = Backing().Submit(std::move(commands));
    if (!done) { return done; }
    const auto status = Require(Backing().Wait(*done, std::chrono::seconds(5)));
    CHECK(status == Render::Native::CompletionStatus::Ready,
          "the offline fixture drains its upload within the explicit budget");
    if (status != Render::Native::CompletionStatus::Ready) { Failed(); }
    return done;
  }
};

inline void CheckSceneUpload(bool result, const std::string &error) {
  if (!result) { std::println(stderr, "{}", error); }
  CHECK(result, "the existing scene upload API accepts the native resource operation");
  if (!result) { Failed(); }
}
}
#endif
