#ifdef __linux__
#include "Context.h"
#include "Surface.h"
#include <array>
#include <string_view>
#include <thread>
#include <print>
#include <expected>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render::Native::Vulkan {

bool OnOwner(const Context &context) noexcept {
  return context.OwnerThread == std::this_thread::get_id();
}

Result<void> CheckOwner(const Context &context) {
  if (!OnOwner(context)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "Vulkan operations require their creating render thread"});
  }
  return {};
}

Error Failure(std::string_view operation, VkResult result) {
  auto kind = ErrorKind::Driver;
  if (result == VK_ERROR_DEVICE_LOST) { kind = ErrorKind::DeviceLost; }
  if (result == VK_ERROR_EXTENSION_NOT_PRESENT || result == VK_ERROR_FEATURE_NOT_PRESENT) {
    kind = ErrorKind::Unsupported;
  }
  if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY) {
    kind = ErrorKind::OutOfMemory;
  }
  return {.Kind = kind, .Message = std::string(operation) + ": VkResult " + std::to_string(result)};
}

Context::~Context() {
  if (Logical != VK_NULL_HANDLE) { vkDestroyDevice(Logical, nullptr); }
  if (Debug != VK_NULL_HANDLE) {
    const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy != nullptr) { destroy(Instance, Debug, nullptr); }
  }
  if (Surface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(Instance, Surface, nullptr); }
  if (Instance != VK_NULL_HANDLE) { vkDestroyInstance(Instance, nullptr); }
}

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL
ReportValidation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                 [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type,
                 const VkDebugUtilsMessengerCallbackDataEXT *message,
                 void *user) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    static_cast<Context *>(user)->ValidationErrors.fetch_add(1, std::memory_order_relaxed);
  }
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::println(stderr,
                 "Vulkan validation [{}]: {}",
                 message->pMessageIdName == nullptr ? "unnamed" : message->pMessageIdName,
                 message->pMessage);
  }
  return VK_FALSE;
}

Result<void> InitInstance(Context &context, DeviceOptions options) {
  if (auto prepared = PrepareInstanceExtensions(context, options.Window); !prepared) {
    return prepared;
  }
  std::vector<const char *> extensions;
  extensions.reserve(context.InstanceExtensions.size() + 1);
  for (const auto &name : context.InstanceExtensions) { extensions.push_back(name.c_str()); }
  VkApplicationInfo application{};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "Outshine";
  application.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &application;
  const char *validation = "VK_LAYER_KHRONOS_validation";

  if (options.Validation) {
    create.enabledLayerCount = 1;
    create.ppEnabledLayerNames = &validation;
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create.ppEnabledExtensionNames = extensions.data();
  const auto result = vkCreateInstance(&create, nullptr, &context.Instance);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateInstance", result)); }
  context.DebugMarkers = options.Validation;
  if (!options.Validation) { return {}; }
  VkDebugUtilsMessengerCreateInfoEXT messages{};
  messages.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messages.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messages.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messages.pfnUserCallback = ReportValidation;
  messages.pUserData = &context;
  const auto make = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(context.Instance, "vkCreateDebugUtilsMessengerEXT"));
  if (make == nullptr) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan debug-utils entry point is unavailable"});
  }
  const auto made = make(context.Instance, &messages, nullptr, &context.Debug);
  if (made != VK_SUCCESS) {
    return std::unexpected(Failure("vkCreateDebugUtilsMessengerEXT", made));
  }
  return {};
}

struct QueueSelection {
  uint32_t Family = 0;
  uint32_t Count = 0;
  uint32_t TimestampValidBits = 0;
};

std::optional<QueueSelection> SelectQueue(const Context &context, VkPhysicalDevice physical) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
  std::vector<VkQueueFamilyProperties> queues(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
  constexpr auto required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
  const bool window = context.Surface != VK_NULL_HANDLE;
  for (uint32_t wanted = context.WsiEnabled ? 2u : 1u; wanted > 0; --wanted) {
    if (window && wanted < 2) { break; }
    for (uint32_t at = 0; at < count; ++at) {
      VkBool32 presents = VK_TRUE;
      if (window && vkGetPhysicalDeviceSurfaceSupportKHR(
                        physical, at, context.Surface, &presents) != VK_SUCCESS) {
        continue;
      }
      if (queues[at].queueCount >= wanted && presents != VK_FALSE &&
          (queues[at].queueFlags & required) == required) {
        return QueueSelection{.Family = at,
                              .Count = queues[at].queueCount,
                              .TimestampValidBits = queues[at].timestampValidBits};
      }
    }
  }
  return std::nullopt;
}

Result<void> SelectPhysical(Context &context) {
  uint32_t count = 0;
  auto result = vkEnumeratePhysicalDevices(context.Instance, &count, nullptr);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkEnumeratePhysicalDevices", result));
  }
  std::vector<VkPhysicalDevice> devices(count);
  result = vkEnumeratePhysicalDevices(context.Instance, &count, devices.data());
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkEnumeratePhysicalDevices", result));
  }
  for (auto *const physical : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    const auto queue = SelectQueue(context, physical);
    if (properties.apiVersion < VK_API_VERSION_1_3 || !queue) { continue; }
    if (context.Physical != VK_NULL_HANDLE &&
        properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      continue;
    }
    context.Physical = physical;
    context.Properties = properties;
    context.QueueFamily = queue->Family;
    context.QueueCount = queue->Count;
    context.TimestampValidBits = queue->TimestampValidBits;
  }
  if (context.Physical == VK_NULL_HANDLE) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan 1.3 graphics/compute device is unavailable"});
  }
  vkGetPhysicalDeviceMemoryProperties(context.Physical, &context.MemoryProperties);
  return {};
}

Result<void> InitLogical(Context &context) {
  VkPhysicalDeviceVulkan13Features supported{};
  supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{};
  maintenance.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
  const bool candidate =
      context.WsiEnabled && context.QueueCount >= 2 && HasPresentExtensions(context.Physical);
  if (candidate) { supported.pNext = &maintenance; }
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &supported;
  vkGetPhysicalDeviceFeatures2(context.Physical, &features);
  if (supported.synchronization2 == VK_FALSE || supported.dynamicRendering == VK_FALSE) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported,
              .Message = "Vulkan synchronization2/dynamicRendering is unavailable"});
  }
  context.PresentCapable = candidate && maintenance.swapchainMaintenance1 != VK_FALSE;
  if (context.Surface != VK_NULL_HANDLE && !context.PresentCapable) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "Vulkan swapchain maintenance1 unavailable"});
  }
  VkPhysicalDeviceVulkan13Features enabled{};
  enabled.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  enabled.synchronization2 = VK_TRUE;
  enabled.dynamicRendering = VK_TRUE;
  if (context.PresentCapable) { enabled.pNext = &maintenance; }
  VkPhysicalDeviceFeatures core{};
  core.robustBufferAccess = features.features.robustBufferAccess;
  core.independentBlend = features.features.independentBlend;
  core.multiDrawIndirect = features.features.multiDrawIndirect;
  core.drawIndirectFirstInstance = features.features.drawIndirectFirstInstance;
  core.depthClamp = features.features.depthClamp;
  context.DepthClamp = core.depthClamp != VK_FALSE;
  context.IndependentBlend = core.independentBlend != VK_FALSE;
  context.MultiDrawIndirect = core.multiDrawIndirect != VK_FALSE;
  context.IndirectFirstInstance = core.drawIndirectFirstInstance != VK_FALSE;
  core.shaderStorageImageExtendedFormats = features.features.shaderStorageImageExtendedFormats;
  context.StorageImageExtendedFormats = core.shaderStorageImageExtendedFormats != VK_FALSE;
  constexpr std::array priorities{1.0f, .5f};
  constexpr std::array extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                  VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
  VkDeviceQueueCreateInfo queue{};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue.queueFamilyIndex = context.QueueFamily;
  queue.queueCount = context.PresentCapable ? 2u : 1u;
  queue.pQueuePriorities = priorities.data();
  VkDeviceCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create.pNext = &enabled;
  create.pEnabledFeatures = &core;
  create.queueCreateInfoCount = 1;
  create.pQueueCreateInfos = &queue;
  if (context.PresentCapable) {
    create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create.ppEnabledExtensionNames = extensions.data();
  }
  const auto result = vkCreateDevice(context.Physical, &create, nullptr, &context.Logical);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateDevice", result)); }
  vkGetDeviceQueue(context.Logical, context.QueueFamily, 0, &context.Queue);
  if (context.PresentCapable) {
    vkGetDeviceQueue(context.Logical, context.QueueFamily, 1, &context.PresentQueue);
    context.ReleaseImages = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(
        vkGetDeviceProcAddr(context.Logical, "vkReleaseSwapchainImagesEXT"));
    if (context.ReleaseImages == nullptr) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported, .Message = "Vulkan swapchain release unavailable"});
    }
  }
  return {};
}

void ReadLimits(Context &context) {
  const auto &limits = context.Properties.limits;
  context.Limits = {.Name = context.Properties.deviceName,
                    .MaxColourAttachments = limits.maxColorAttachments,
                    .MaxTextureExtent = limits.maxImageDimension2D,
                    .MaxTextureLayers = limits.maxImageArrayLayers,
                    .StorageBufferAlignment = limits.minStorageBufferOffsetAlignment,
                    .UniformBufferAlignment = limits.minUniformBufferOffsetAlignment,
                    .MaxStorageBufferBytes = limits.maxStorageBufferRange,
                    .MaxUniformBufferBytes = limits.maxUniformBufferRange,
                    .MaxWorkgroupSize = {limits.maxComputeWorkGroupSize[0],
                                         limits.maxComputeWorkGroupSize[1],
                                         limits.maxComputeWorkGroupSize[2]},
                    .MaxWorkgroupCount = {limits.maxComputeWorkGroupCount[0],
                                          limits.maxComputeWorkGroupCount[1],
                                          limits.maxComputeWorkGroupCount[2]},
                    .MaxWorkgroupInvocations = limits.maxComputeWorkGroupInvocations,
                    .GpuTimestamps = context.TimestampValidBits != 0,
                    .DepthClamp = context.DepthClamp};
}

}

Result<std::shared_ptr<Context>> CreateContext(DeviceOptions options) {
  static std::atomic<uint64_t> nextCookie{1};
  auto context = std::make_shared<Context>();
  context->Cookie = nextCookie.fetch_add(1, std::memory_order_relaxed);
  if (auto result = InitInstance(*context, options); !result) {
    return std::unexpected(result.error());
  }
  if (auto result = CreateSurface(*context, options.Window); !result) {
    return std::unexpected(result.error());
  }
  if (auto result = SelectPhysical(*context); !result) { return std::unexpected(result.error()); }
  if (auto result = InitLogical(*context); !result) { return std::unexpected(result.error()); }
  ReadLimits(*context);
  context->GpuTiming = options.GpuTiming && context->Limits.GpuTimestamps;
  return context;
}

Result<uint32_t> FindMemory(const Context &context, uint32_t allowed, MemoryChoice choice) {
  uint32_t chosen = context.MemoryProperties.memoryTypeCount;
  int best = -1;
  for (uint32_t at = 0; at < context.MemoryProperties.memoryTypeCount; ++at) {
    const auto flags = context.MemoryProperties.memoryTypes[at].propertyFlags;
    if ((allowed & (1u << at)) == 0 || (flags & choice.Required) != choice.Required) { continue; }
    const auto score = std::popcount(flags & choice.Preferred);
    if (score > best) {
      chosen = at;
      best = score;
    }
  }
  if (best < 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "no compatible Vulkan memory type"});
  }
  return chosen;
}

}
#endif
