#ifdef __linux__
#include "Image.h"
#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace outshine::Render::Native::Vulkan {

SamplerState::~SamplerState() {
  if (Handle != VK_NULL_HANDLE) { vkDestroySampler(Owner->Logical, Handle, nullptr); }
}

namespace {

std::optional<VkSamplerAddressMode> AddressMode(Address address) {
  switch (address) {
    case Address::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Address::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case Address::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  return std::nullopt;
}

}

Result<std::shared_ptr<SamplerState>> AllocateSampler(std::shared_ptr<Context> context,
                                                      SamplerDescription description) {
  if (auto owner = CheckOwner(*context); !owner) { return std::unexpected(owner.error()); }
  const auto u = AddressMode(description.U);
  const auto v = AddressMode(description.V);
  const auto filterValid = [](Filter filter) {
    return filter == Filter::Linear || filter == Filter::Nearest;
  };
  if (!u || !v || !filterValid(description.Min) || !filterValid(description.Mag) ||
      !filterValid(description.Mip) || !std::isfinite(description.MinLod) ||
      !std::isfinite(description.MaxLod) || description.MinLod < 0 ||
      description.MinLod > description.MaxLod) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid sampler filters or LOD range"});
  }
  auto state = std::make_shared<SamplerState>();
  state->Owner = std::move(context);
  state->Filtered = description.Min == Filter::Linear || description.Mag == Filter::Linear ||
                    description.Mip == Filter::Linear;
  VkSamplerCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  create.minFilter = description.Min == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  create.magFilter = description.Mag == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  create.mipmapMode = description.Mip == Filter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                                        : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  create.addressModeU = *u;
  create.addressModeV = *v;
  create.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  create.minLod = description.MinLod;
  create.maxLod = description.MaxLod;
  const auto result = vkCreateSampler(state->Owner->Logical, &create, nullptr, &state->Handle);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateSampler", result)); }
  return state;
}

}
#endif
