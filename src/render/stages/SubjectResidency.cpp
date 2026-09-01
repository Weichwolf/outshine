#include <atomic>
#include "SubjectResidency.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <cstring>
#include <utility>
#include <vector>

namespace outshine::Render {

namespace Says {
inline constexpr std::string_view kStreamFoundNoRoom =
    "a vertex stream found no room on the device: {}";
inline constexpr std::string_view kPoseStagingFoundNoRoom =
    "the pose's staging buffer found no room on the device: {}";
inline constexpr std::string_view kPoseStagingDidNotMap =
    "the pose's staging buffer did not map: {}";
inline constexpr std::string_view kTopologyStagingFoundNoRoom =
    "the topology's staging buffer found no room on the device: {}";
inline constexpr std::string_view kTopologyStagingDidNotMap =
    "the topology's staging buffer did not map: {}";
} // namespace Says

namespace {

float LinearFromSrgb8(uint8_t code) {
  const float encoded = static_cast<float>(code) * (1.0f / 255.0f);
  if (encoded < 0.04045f) { return encoded * (1.0f / 12.92f); }
  return std::pow((encoded + 0.055f) * (1.0f / 1.055f), 2.4f);
}

SDL_GPUSamplerAddressMode AddressOf(SubjectWrap wrap) {
  switch (wrap) {
    case SubjectWrap::ClampToEdge: return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    case SubjectWrap::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    case SubjectWrap::Repeat: return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  }
  return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
}

SDL_GPUFilter FilterOf(SubjectFilter filter) {
  return filter == SubjectFilter::Nearest ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
}

constexpr bool kChainIsReadable = false;

} // namespace

static std::atomic<size_t> gUploads{0};
static std::atomic<size_t> gUploadsEver{0};
static std::atomic<size_t> gCrossingsFlushed{0};
static std::atomic<size_t> gUploadBytes{0};
static std::atomic<size_t> gBuffersMade{0};
static std::atomic<size_t> gStagingMade{0};

size_t SubjectResidency::UploadsTaken() {
  return gUploads.exchange(0u);
}

size_t SubjectResidency::UploadsEver() {
  return gUploadsEver.load(std::memory_order_relaxed);
}

size_t SubjectResidency::CrossingsFlushed() {
  return gCrossingsFlushed.load(std::memory_order_relaxed);
}

size_t SubjectResidency::UploadMBTaken() {
  return gUploadBytes.exchange(0u) / 1000000u;
}

size_t SubjectResidency::BuffersMadeTaken() {
  return gBuffersMade.exchange(0u);
}

size_t SubjectResidency::StagingMadeTaken() {
  return gStagingMade.exchange(0u);
}

bool SubjectResidency::Cross(Crossing *what, size_t count, bool deferred, std::string &error) {
  uint32_t total = 0;
  for (size_t at = 0; at < count; ++at) {
    const Crossing &one = what[at];
    if (one.Bytes == 0 || !one.Stands()) {
      one.Into->Reset();
      *one.Held = 0;
      continue;
    }
    if (*one.Held < one.Bytes || !*one.Into) {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage = one.Usage;
      wanted.size = one.Bytes;
      *one.Into = OwnedBuffer(Device, SDL_CreateGPUBuffer(Device, &wanted));
      gBuffersMade.fetch_add(1u, std::memory_order_relaxed);
      if (!*one.Into) {
        *one.Held = 0;
        error = std::format(Says::kStreamFoundNoRoom, SDL_GetError());
        return false;
      }
      *one.Held = one.Bytes;
    }

    total = (total + one.Bytes + 15u) & ~15u;
  }
  if (total == 0) { return true; }

  if (!deferred) { return Submit(what, count, total, error); }
  if (StagingUsed_ + total > StagingBytes_ || !Staging_) {
    const uint32_t widened = total > StagingBytes_ ? total : StagingBytes_;
    SDL_GPUTransferBufferCreateInfo room{};
    room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    room.size = widened;
    OwnedTransfer fresh(Device, SDL_CreateGPUTransferBuffer(Device, &room));
    if (!fresh) {
      error = std::format(Says::kPoseStagingFoundNoRoom, SDL_GetError());
      return false;
    }
    if (Staging_ && StagedCount_ > 0) { Retired_.push_back(std::move(Staging_)); }
    Staging_ = std::move(fresh);
    StagingBytes_ = widened;
    StagingUsed_ = 0;
  }

  auto *const mapped =
      static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device, Staging_.Get(), StagingUsed_ == 0));
  if (mapped == nullptr) {
    error = std::format(Says::kPoseStagingDidNotMap, SDL_GetError());
    return false;
  }
  uint32_t at = StagingUsed_;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || !what[one].Stands()) { continue; }
    if (what[one].Writes != nullptr) {
      what[one].Writes(what[one].Carrying,
                       reinterpret_cast<float *>(mapped + at),
                       what[one].Bytes / static_cast<uint32_t>(sizeof(float)));
    } else {
      std::memcpy(mapped + at, what[one].From, what[one].Bytes);
    }
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device, Staging_.Get());

  at = StagingUsed_;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || !what[one].Stands()) { continue; }
    if (StagedCount_ == Staged_.size()) { Staged_.push_back(Staged{}); }
    Staged_[StagedCount_++] = Staged{.Into = what[one].Into->Get(),
                                     .From = at,
                                     .Bytes = what[one].Bytes,
                                     .Staging = Staging_.Get()};
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  StagingUsed_ = at;
  StagedThisFrame_ += at;
  return true;
}

bool SubjectResidency::Submit(Crossing *what, size_t count, uint32_t total, std::string &error) {
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = total;
  if (BulkBytes_ < total || !Bulk_) {
    Bulk_ = OwnedTransfer(Device, SDL_CreateGPUTransferBuffer(Device, &room));
    BulkBytes_ = Bulk_ ? total : 0u;
    gStagingMade.fetch_add(1u, std::memory_order_relaxed);
  }
  gUploads.fetch_add(1u, std::memory_order_relaxed);
  gUploadsEver.fetch_add(1u, std::memory_order_relaxed);
  gUploadBytes.fetch_add(total, std::memory_order_relaxed);
  if (!Bulk_) {
    error = std::format(Says::kTopologyStagingFoundNoRoom, SDL_GetError());
    return false;
  }
  auto *const mapped = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device, Bulk_.Get(), true));
  if (mapped == nullptr) {
    error = std::format(Says::kTopologyStagingDidNotMap, SDL_GetError());
    return false;
  }
  uint32_t at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || !what[one].Stands()) { continue; }
    if (what[one].Writes != nullptr) {
      what[one].Writes(what[one].Carrying,
                       reinterpret_cast<float *>(mapped + at),
                       what[one].Bytes / static_cast<uint32_t>(sizeof(float)));
    } else {
      std::memcpy(mapped + at, what[one].From, what[one].Bytes);
    }
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device, Bulk_.Get());

  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || !what[one].Stands()) { continue; }
    const SDL_GPUTransferBufferLocation source{.transfer_buffer = Bulk_.Get(), .offset = at};
    const SDL_GPUBufferRegion into{
        .buffer = what[one].Into->Get(), .offset = 0, .size = what[one].Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, false);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  return true;
}

void SubjectResidency::FlushCrossings(SDL_GPUCommandBuffer *commands) {
  if (StagedCount_ == 0 || commands == nullptr) { return; }
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  for (size_t at = 0; at < StagedCount_; ++at) {
    const SDL_GPUTransferBufferLocation source{.transfer_buffer = Staged_[at].Staging,
                                               .offset = Staged_[at].From};
    const SDL_GPUBufferRegion into{
        .buffer = Staged_[at].Into, .offset = 0, .size = Staged_[at].Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, false);
    gCrossingsFlushed.fetch_add(1u, std::memory_order_relaxed);
  }
  SDL_EndGPUCopyPass(copy);
  StagedCount_ = 0;
  StagingUsed_ = 0;
  Retired_.clear();
}

SubjectResidency::BoundImage
SubjectResidency::Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind) const {
  static const uint8_t white[4] = {255, 255, 255, 255};
  const uint32_t width = texture.Width > 0 ? texture.Width : 1;
  const uint32_t height = texture.Height > 0 ? texture.Height : 1;
  const uint8_t *texels = (texture.Rgba != nullptr) ? texture.Rgba : white;
  std::vector<float> linear(static_cast<size_t>(width) * height * 4u, 0.0f);
  for (size_t texel = 0; texel < linear.size() / 4u; ++texel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      const uint8_t code = texels[texel * 4u + channel];
      linear[texel * 4u + channel] =
          decode == Transfer::Srgb ? LinearFromSrgb8(code) : static_cast<float>(code) / 255.0f;
    }
    linear[texel * 4u + 3u] = static_cast<float>(texels[texel * 4u + 3u]) / 255.0f;
  }

  if (kind == TexelKind::Direction) {
    for (size_t texel = 0; texel < linear.size() / 4u; ++texel) { linear[texel * 4u + 3u] = 1.0f; }
  }

  BoundImage bound;
  SDL_GPUTextureCreateInfo wantedTexture{};
  wantedTexture.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTexture.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedTexture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTexture.width = width;
  wantedTexture.height = height;
  wantedTexture.layer_count_or_depth = 1;

  uint32_t levels = 1;
  for (uint32_t extent = width > height ? width : height; extent > 1u; extent /= 2u) { ++levels; }

  if (texture.Mip == SubjectMip::None || !kChainIsReadable) { levels = 1; }
  wantedTexture.num_levels = levels;
  wantedTexture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  bound.Image = OwnedTexture(Device, SDL_CreateGPUTexture(Device, &wantedTexture));

  const uint32_t indexChannels = kind == TexelKind::Direction ? 0u : IndexChannelsOf(linear);
  std::vector<float> level = linear;
  uint32_t levelWidth = width;
  uint32_t levelHeight = height;
  for (uint32_t which = 0; which < levels; ++which) {
    if (which > 0) {
      std::vector<float> smaller;
      uint32_t smallerWidth = 0;
      uint32_t smallerHeight = 0;
      HalveInPlace(level,
                   levelWidth,
                   levelHeight,
                   smaller,
                   smallerWidth,
                   smallerHeight,
                   kind,
                   indexChannels);
      level.swap(smaller);
      levelWidth = smallerWidth;
      levelHeight = smallerHeight;
    }
    const uint32_t bytes = levelWidth * levelHeight * 4u * static_cast<uint32_t>(sizeof(float));
    SDL_GPUTransferBufferCreateInfo wantedTransfer{};
    wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    wantedTransfer.size = bytes;
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(Device, &wantedTransfer);
    gStagingMade.fetch_add(1u, std::memory_order_relaxed);
    gUploads.fetch_add(1u, std::memory_order_relaxed);
    gUploadsEver.fetch_add(1u, std::memory_order_relaxed);
    gUploadBytes.fetch_add(bytes, std::memory_order_relaxed);
    void *const mappedLevel = SDL_MapGPUTransferBuffer(Device, staging, false);
    if (mappedLevel == nullptr) {
      SDL_ReleaseGPUTransferBuffer(Device, staging);
      bound.Image.Reset();
      return bound;
    }
    std::memcpy(mappedLevel, level.data(), bytes);
    SDL_UnmapGPUTransferBuffer(Device, staging);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.pixels_per_row = levelWidth;
    source.rows_per_layer = levelHeight;
    SDL_GPUTextureRegion into{};
    into.texture = bound.Image.Get();
    into.mip_level = which;
    into.w = levelWidth;
    into.h = levelHeight;
    into.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &into, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(Device, staging);
  }

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.address_mode_u = AddressOf(texture.WrapU);
  wantedSampler.address_mode_v = AddressOf(texture.WrapV);
  wantedSampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  wantedSampler.min_filter = FilterOf(texture.Minify);
  wantedSampler.mag_filter = FilterOf(texture.Magnify);

  wantedSampler.mipmap_mode = texture.Mip == SubjectMip::Nearest ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                                                 : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

  wantedSampler.max_lod = 1000.0f;
  bound.Sample = OwnedSampler(Device, SDL_CreateGPUSampler(Device, &wantedSampler));
  return bound;
}

} // namespace outshine::Render
