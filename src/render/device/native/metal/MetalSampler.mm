#include "MetalInternal.h"
#include <cmath>

namespace outshine::Render::Native::Metal {
namespace {

std::optional<MTLSamplerAddressMode> Addressing(Address address) {
  switch (address) {
    case Address::Repeat: return MTLSamplerAddressModeRepeat;
    case Address::MirrorRepeat: return MTLSamplerAddressModeMirrorRepeat;
    case Address::ClampToEdge: return MTLSamplerAddressModeClampToEdge;
  }
  return std::nullopt;
}

}

Result<std::unique_ptr<Sampler>> MetalDevice::CreateSampler(SamplerDescription description,
                                                            std::string_view label) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto u = Addressing(description.U);
    const auto v = Addressing(description.V);
    const auto valid = [](Filter filter) {
      return filter == Filter::Nearest || filter == Filter::Linear;
    };
    if (!u || !v || !valid(description.Min) || !valid(description.Mag) || !valid(description.Mip) ||
        !std::isfinite(description.MinLod) || !std::isfinite(description.MaxLod) ||
        description.MinLod < 0 || description.MaxLod < description.MinLod) {
      return Failure(ErrorKind::Invalid, "invalid Metal sampler filter, addressing or LOD range");
    }
    MTLSamplerDescriptor *const info = [MTLSamplerDescriptor new];
    info.sAddressMode = *u;
    info.tAddressMode = *v;
    info.rAddressMode = MTLSamplerAddressModeClampToEdge;
    info.minFilter = description.Min == Filter::Linear ? MTLSamplerMinMagFilterLinear
                                                       : MTLSamplerMinMagFilterNearest;
    info.magFilter = description.Mag == Filter::Linear ? MTLSamplerMinMagFilterLinear
                                                       : MTLSamplerMinMagFilterNearest;
    info.mipFilter =
        description.Mip == Filter::Linear ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    info.label = Text(label);
    info.lodMinClamp = description.MinLod;
    info.lodMaxClamp = description.MaxLod;
    const id<MTLSamplerState> sampler = [Native_ newSamplerStateWithDescriptor:info];
    if (sampler == nil) { return Failure(ErrorKind::Driver, "Metal sampler creation failed"); }
    return std::make_unique<MetalSampler>(sampler,
                                          Identity_,
                                          description.Min == Filter::Linear ||
                                              description.Mag == Filter::Linear ||
                                              description.Mip == Filter::Linear);
  }
}

}
