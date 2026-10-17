#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_PRESENTER_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_PRESENTER_H
#ifdef __linux__
#include "VulkanImage.h"
#include "Surface.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <vector>

namespace outshine::Render::Native::Vulkan {

struct SwapchainState {
  std::shared_ptr<Context> Owner;
  std::shared_ptr<SurfaceState> Surface;
  VkSwapchainKHR Handle = VK_NULL_HANDLE;
  bool DriverRetired = false;
  VkExtent2D Extent{};
  Format Texels = Format::Bgra8Unorm;
  UseMask<TextureUse> Use = TextureUse::ColourAttachment;
  uint64_t Generation = 0;
  std::vector<VkImage> Images;
  std::vector<VkImageLayout> Layouts;
  SwapchainState() = default;
  ~SwapchainState();
  SwapchainState(const SwapchainState &) = delete;
  SwapchainState &operator=(const SwapchainState &) = delete;
};

enum class PresentPhase {
  Acquiring,
  Ready,
  Acquired,
  Recorded,
  Submitted,
  Queued,
  Cancelled,
  Released
};
struct PresentState;

struct PresentLease {
  std::shared_ptr<SwapchainState> Target;
  std::weak_ptr<PresentState> Dispatcher;
  uint32_t Index = 0;
  uint64_t Generation = 0;
  VkSemaphore Acquired = VK_NULL_HANDLE;
  VkSemaphore Rendered = VK_NULL_HANDLE;
  VkFence Available = VK_NULL_HANDLE;
  VkFence Presented = VK_NULL_HANDLE;
  VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  std::atomic<PresentPhase> Phase{PresentPhase::Acquiring};
  std::atomic<uint32_t> Recordings{0};
  PresentLease() = default;
  ~PresentLease();
  PresentLease(const PresentLease &) = delete;
  PresentLease &operator=(const PresentLease &) = delete;
  void Wake() const noexcept;
  [[nodiscard]] bool Current() const noexcept;
};

struct TargetRequest {
  std::shared_ptr<SurfaceState> Surface;
  VkExtent2D Extent{};
  std::optional<Error> Failure;
  std::atomic<bool> Done{false};
};

struct PresentState {
  std::shared_ptr<Context> Owner;
  std::mutex Gate;
  std::condition_variable Changed;
  std::shared_ptr<SurfaceState> Surface;
  std::shared_ptr<TargetRequest> RequestedTarget;
  uint32_t Width = 0;
  uint32_t Height = 0;
  std::atomic<uint64_t> Generation{1};
  std::optional<Error> Failed;
  std::array<std::shared_ptr<PresentLease>, 3> Slots;
  std::atomic<bool> Stop{false};
  std::atomic<uint64_t> Revision{0};
  void Wake() noexcept;
  std::shared_ptr<SwapchainState> Active;
  std::vector<std::shared_ptr<SwapchainState>> Inactive;
  std::shared_ptr<PresentLease> Attempt;
};

class VulkanPresentFrame final : public PresentFrame {
public:
  explicit VulkanPresentFrame(std::shared_ptr<PresentLease> lease);
  ~VulkanPresentFrame() override;

  [[nodiscard]] const Texture &Image() const noexcept override { return *Image_; }

  [[nodiscard]] const std::shared_ptr<PresentLease> &Lease() const noexcept { return Lease_; }

private:
  std::shared_ptr<PresentLease> Lease_;
  std::unique_ptr<VulkanTexture> Image_;
};

class Presenter {
public:
  explicit Presenter(std::shared_ptr<Context> context);
  ~Presenter();
  Presenter(const Presenter &) = delete;
  Presenter &operator=(const Presenter &) = delete;
  [[nodiscard]] Result<void> Start();
  [[nodiscard]] Result<void> SetTarget(SDL_Window *window);
  void Stop();
  [[nodiscard]] Result<std::unique_ptr<PresentFrame>> Acquire();

private:
  static void *Entry(void *self);
  SDL_Window *Window_ = nullptr;
  std::shared_ptr<PresentState> State_;
  pthread_t Worker_{};
  bool Started_ = false;
};

[[nodiscard]] Result<std::shared_ptr<SwapchainState>>
CreateSwapchain(std::shared_ptr<SurfaceState> surface,
                VkExtent2D extent,
                uint64_t generation,
                const std::shared_ptr<SwapchainState> &before);
[[nodiscard]] Result<VkSurfaceCapabilitiesKHR> ReadSurfaceCapabilities(const SurfaceState &surface);
[[nodiscard]] Result<std::shared_ptr<PresentLease>>
AcquireLease(const std::shared_ptr<PresentState> &state,
             const std::shared_ptr<SwapchainState> &target);
void RunPresenter(const std::shared_ptr<PresentState> &state);
void DrainPresenter(const std::shared_ptr<PresentState> &state);
void FailPresentation(PresentState &state, Error error, uint64_t generation);
void SwitchTarget(const std::shared_ptr<PresentState> &state);
void InvalidatePresentation(PresentState &state, uint64_t generation);
[[nodiscard]] Result<bool> AdvanceLease(const std::shared_ptr<PresentLease> &lease, bool stopping);

}
#endif
#endif
