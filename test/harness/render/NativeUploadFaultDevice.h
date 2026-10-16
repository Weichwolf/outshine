#ifndef OUTSHINE_TEST_RENDER_NATIVEUPLOADFAULTDEVICE_H
#define OUTSHINE_TEST_RENDER_NATIVEUPLOADFAULTDEVICE_H

#include "NativeDeviceProxy.h"
#include <cstddef>
#include <expected>
#include <span>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Test::Gpu {
enum class UploadFailurePoint { Map, Submit };

struct UploadFailure {
  std::string Message = "injected native submission failure";
  UploadFailurePoint Point = UploadFailurePoint::Submit;
  bool Armed = false;
  unsigned Rejected = 0;
};

class NativeUploadFaultDevice final : public NativeDeviceProxy {
public:
  NativeUploadFaultDevice(std::unique_ptr<Render::Native::Device> device, UploadFailure &failure)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Failure_(failure) {}

  [[nodiscard]] static Render::Native::Result<std::unique_ptr<Render::Native::Device>>
  Create(void *context, Render::Native::DeviceOptions options) {
    auto device = Render::Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<NativeUploadFaultDevice>(std::move(*device),
                                                     *static_cast<UploadFailure *>(context));
  }

  [[nodiscard]] Render::Native::Result<std::span<std::byte>>
  MapWrite(Render::Native::Buffer &buffer, Render::Native::ByteRange range) override {
    if (Reject(UploadFailurePoint::Map)) { return std::unexpected(Injected()); }
    return Backing().MapWrite(buffer, range);
  }

  [[nodiscard]] Render::Native::Result<Render::Native::Completion>
  Submit(std::unique_ptr<Render::Native::Commands> commands) override {
    if (Reject(UploadFailurePoint::Submit)) { return std::unexpected(Injected()); }
    return Backing().Submit(std::move(commands));
  }

private:
  [[nodiscard]] bool Reject(UploadFailurePoint point) {
    if (!Failure_.Armed || Failure_.Point != point) { return false; }
    Failure_.Armed = false;
    ++Failure_.Rejected;
    return true;
  }

  [[nodiscard]] Render::Native::Error Injected() const {
    return {.Kind = Render::Native::ErrorKind::Driver, .Message = Failure_.Message};
  }

  std::unique_ptr<Render::Native::Device> Device_;
  UploadFailure &Failure_;
};
}
#endif
