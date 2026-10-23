#include "Check.h"
#include "src/render/device/native/vulkan/VulkanImage.h"

#include <array>
#include <cstdint>

namespace {
using outshine::Render::Native::Vulkan::ImageReadScope;
using namespace outshine::Test;

constexpr VkImageSubresourceRange kRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1};
constexpr ImageReadScope::Access kShaderRead{.Stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             .Access = VK_ACCESS_2_SHADER_READ_BIT,
                                             .Layout = VK_IMAGE_LAYOUT_GENERAL};

void CheckRanges() {
  ImageReadScope scope;
  CHECK(!scope.Repeats(kRange, kShaderRead), "the first image use needs synchronization");
  CHECK(scope.Repeats(kRange, kShaderRead), "an identical synchronized image read repeats");
  constexpr std::array members{&VkImageSubresourceRange::aspectMask,
                               &VkImageSubresourceRange::baseMipLevel,
                               &VkImageSubresourceRange::levelCount,
                               &VkImageSubresourceRange::baseArrayLayer,
                               &VkImageSubresourceRange::layerCount};
  for (const auto member : members) {
    auto changed = kRange;
    ++(changed.*member);
    CHECK(!scope.Repeats(changed, kShaderRead), "each subresource or aspect change synchronizes");
    CHECK(scope.Repeats(changed, kShaderRead), "the new exact subresource can repeat");
    CHECK(!scope.Repeats(kRange, kShaderRead), "a single scope does not retain an older range");
    CHECK(scope.Repeats(kRange, kShaderRead), "the restored exact range can repeat");
  }
}

void CheckScopes() {
  ImageReadScope scope;
  CHECK(!scope.Repeats(kRange, kShaderRead), "a new scope starts unsynchronized");
  auto stage = kShaderRead;
  stage.Stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  auto access = kShaderRead;
  access.Access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  auto layout = kShaderRead;
  layout.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  for (const auto changed : {stage, access, layout}) {
    CHECK(!scope.Repeats(kRange, changed), "stage, access and layout must each match exactly");
    CHECK(scope.Repeats(kRange, changed), "the new exact read scope can repeat");
    CHECK(!scope.Repeats(kRange, kShaderRead), "returning to an older scope synchronizes again");
  }
  scope.Reset();
  CHECK(!scope.Repeats(kRange, kShaderRead),
        "explicit invalidation revokes the remembered image read");
  CHECK(scope.Repeats(kRange, kShaderRead), "the new read after a write can repeat");
  ImageReadScope next;
  CHECK(!next.Repeats(kRange, kShaderRead),
        "a new scope starts independently of an existing remembered read");
  next.Reset();
  CHECK(!next.Repeats(kRange, kShaderRead), "discarded scope state cannot supply a read");
}

void CheckInvalidations() {
  constexpr VkAccessFlags2 kUnknown = VkAccessFlags2{1} << 63u;
  constexpr std::array masks{
      VkAccessFlags2{0},
      VkAccessFlags2{VK_ACCESS_2_SHADER_WRITE_BIT},
      VkAccessFlags2{VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT},
      VkAccessFlags2{VK_ACCESS_2_MEMORY_READ_BIT},
      kUnknown,
      kUnknown | VK_ACCESS_2_SHADER_READ_BIT};
  for (const auto mask : masks) {
    ImageReadScope scope;
    CHECK(!scope.Repeats(kRange, kShaderRead), "a first read precedes the invalidating access");
    auto changed = kShaderRead;
    changed.Access = mask;
    CHECK(!scope.Repeats(kRange, changed), "write, empty or unknown access cannot repeat");
    CHECK(!scope.Repeats(kRange, changed), "unqualified access never installs a read witness");
    CHECK(!scope.Repeats(kRange, kShaderRead), "read after invalidating access synchronizes");
    CHECK(scope.Repeats(kRange, kShaderRead), "only the freshly synchronized read repeats");
  }
  ImageReadScope scope;
  CHECK(!scope.Repeats(kRange, kShaderRead), "a first read precedes the empty stage");
  auto changed = kShaderRead;
  changed.Stage = VK_PIPELINE_STAGE_2_NONE;
  CHECK(!scope.Repeats(kRange, changed), "a missing destination stage revokes the read");
  CHECK(!scope.Repeats(kRange, kShaderRead), "a read after the empty stage synchronizes");
}

}

int main() {
  CheckRanges();
  CheckScopes();
  CheckInvalidations();
  return Report();
}
