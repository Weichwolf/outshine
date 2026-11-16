#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_ENCODERPROFILE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_ENCODERPROFILE_H

#ifdef __linux__
#include "Context.h"
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

struct EncoderProfileSession;

[[nodiscard]] bool EncoderProfilingRequested();
[[nodiscard]] std::shared_ptr<EncoderProfileSession> CreateEncoderProfileSession(Context &context);

struct ProfiledPass {
  std::string Label;
  bool Raster = false;
  bool Available = false;
  double GpuNs = std::numeric_limits<double>::quiet_NaN();
  uint64_t DrawCalls = 0;
  std::optional<uint64_t> InputPrimitives;
};

class EncoderProfile final {
public:
  EncoderProfile(Context &context, std::shared_ptr<EncoderProfileSession> session)
      : Context_(context), Session_(std::move(session)) {}

  ~EncoderProfile();
  EncoderProfile(const EncoderProfile &) = delete;
  EncoderProfile &operator=(const EncoderProfile &) = delete;

  void Initialize(VkCommandBuffer commands);
  [[nodiscard]] std::optional<uint32_t>
  Begin(VkCommandBuffer commands, std::string_view label, bool raster);
  void End(VkCommandBuffer commands, std::optional<uint32_t> pass);
  void Draw(uint32_t calls = 1);
  void BeginLabel(std::string_view label);
  void EndLabel();
  void Collect(uint64_t serial);

private:
  static constexpr uint32_t kMaximumPasses = 256;
  Context &Context_;
  std::shared_ptr<EncoderProfileSession> Session_;
  VkQueryPool Timestamps_ = VK_NULL_HANDLE;
  VkQueryPool Primitives_ = VK_NULL_HANDLE;
  std::chrono::steady_clock::time_point RecordedAt_;
  std::vector<ProfiledPass> Passes_;
  std::vector<std::string> Labels_;
  std::optional<uint32_t> Active_;
  uint64_t Dropped_ = 0;
  bool Collected_ = false;
};

}
#endif
#endif
