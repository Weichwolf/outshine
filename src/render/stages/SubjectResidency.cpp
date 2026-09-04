#include <span>
#include <array>
#include <atomic>
#include "SubjectResidency.h"
#include <algorithm>

#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <cstring>
#include <utility>
#include <vector>

namespace outshine::Render {

constexpr float kByteSteps = 255.0f;

constexpr float kSrgbKnee = 0.04045f;
constexpr float kSrgbLinearSlope = 12.92f;
constexpr float kSrgbOffset = 0.055f;
constexpr float kSrgbScale = 1.055f;
constexpr float kSrgbGamma = 2.4f;
constexpr uint64_t kBytesPerMegabyte = 1000000u;
constexpr float kEveryMip = 1000.0f;
constexpr size_t kRgbaChannels = 4u;
constexpr size_t kAlphaChannel = 3u;

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
  const float encoded = static_cast<float>(code) * (1.0f / kByteSteps);
  if (encoded < kSrgbKnee) { return encoded * (1.0f / kSrgbLinearSlope); }
  return std::pow((encoded + kSrgbOffset) * (1.0f / kSrgbScale), kSrgbGamma);
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

std::atomic<size_t> gUploads{0};
std::atomic<size_t> gUploadsEver{0};
std::atomic<size_t> gCrossingsFlushed{0};
std::atomic<size_t> gUploadBytes{0};
std::atomic<size_t> gBuffersMade{0};
std::atomic<size_t> gStagingMade{0};

} // namespace

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
  return gUploadBytes.exchange(0u) / kBytesPerMegabyte;
}

size_t SubjectResidency::BuffersMadeTaken() {
  return gBuffersMade.exchange(0u);
}

size_t SubjectResidency::StagingMadeTaken() {
  return gStagingMade.exchange(0u);
}

SubjectResidency::Range
SubjectResidency::Take(std::vector<Range> &free, uint32_t count, uint32_t &top) {
  for (size_t at = 0; at < free.size(); ++at) {
    if (free[at].Count < count) { continue; }
    const Range taken{.First = free[at].First, .Count = count};
    free[at].First += count;
    free[at].Count -= count;
    if (free[at].Count == 0) { free.erase(free.begin() + static_cast<long>(at)); }
    return taken;
  }
  const Range taken{.First = top, .Count = count};
  top += count;
  return taken;
}

void SubjectResidency::Give(std::vector<Range> &free, Range back) {
  if (back.Count == 0) { return; }
  const auto after =
      std::ranges::lower_bound(free, back.First, {}, [](const Range &one) { return one.First; });
  const auto at = free.insert(after, back);
  const size_t here = static_cast<size_t>(at - free.begin());
  if (here + 1 < free.size() && free[here].First + free[here].Count == free[here + 1].First) {
    free[here].Count += free[here + 1].Count;
    free.erase(free.begin() + static_cast<long>(here) + 1);
  }
  if (here > 0 && free[here - 1].First + free[here - 1].Count == free[here].First) {
    free[here - 1].Count += free[here].Count;
    free.erase(free.begin() + static_cast<long>(here));
  }
}

bool SubjectResidency::Cross(std::span<Crossing> what, bool deferred, std::string &error) {
  uint32_t total = 0;
  for (const auto &one : what) {
    OwnedBuffer &into = Buffer(one.Which);
    uint32_t *const stood = HeldAt(one.Which);
    if (one.Bytes == 0 || !one.Stands()) {
      into.Reset();
      *stood = 0;
      continue;
    }
    if (*stood < one.Offset + one.Bytes || !into) {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage = one.Usage;
      wanted.size = one.Offset + one.Bytes;
      into = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
      gBuffersMade.fetch_add(1u, std::memory_order_relaxed);
      if (!into) {
        *stood = 0;
        error = std::format(Says::kStreamFoundNoRoom, SDL_GetError());
        return false;
      }
      *stood = one.Offset + one.Bytes;
    }

    total = (total + one.Bytes + 15u) & ~15u;
  }
  if (total == 0) { return true; }

  if (!deferred) { return Submit(what, total, error); }
  if (StagingUsed_ + total > StagingBytes_ || !Staging_) {
    const uint32_t widened = total > StagingBytes_ ? total : StagingBytes_;
    SDL_GPUTransferBufferCreateInfo room{};
    room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    room.size = widened;
    OwnedTransfer fresh(Device_, SDL_CreateGPUTransferBuffer(Device_, &room));
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
      static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device_, Staging_.Get(), StagingUsed_ == 0));
  if (mapped == nullptr) {
    error = std::format(Says::kPoseStagingDidNotMap, SDL_GetError());
    return false;
  }
  uint32_t at = StagingUsed_;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.Stands()) { continue; }
    if (one.Writes != nullptr) {
      one.Writes(one.Carrying,
                 reinterpret_cast<float *>(mapped + at),
                 one.Bytes / static_cast<uint32_t>(sizeof(float)));
    } else {
      std::memcpy(mapped + at, one.From, one.Bytes);
    }
    at = (at + one.Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device_, Staging_.Get());

  at = StagingUsed_;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.Stands()) { continue; }
    if (StagedCount_ == Staged_.size()) { Staged_.push_back(Staged{}); }
    Staged_[StagedCount_++] = Staged{.Into = Buffer(one.Which).Get(),
                                     .From = at,
                                     .Bytes = one.Bytes,
                                     .Offset = one.Offset,
                                     .Staging = Staging_.Get()};
    at = (at + one.Bytes + 15u) & ~15u;
  }
  StagingUsed_ = at;
  StagedThisFrame_ += at;
  return true;
}

bool SubjectResidency::Submit(std::span<Crossing> what, uint32_t total, std::string &error) {
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = total;
  if (BulkBytes_ < total || !Bulk_) {
    Bulk_ = OwnedTransfer(Device_, SDL_CreateGPUTransferBuffer(Device_, &room));
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
  auto *const mapped = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device_, Bulk_.Get(), true));
  if (mapped == nullptr) {
    error = std::format(Says::kTopologyStagingDidNotMap, SDL_GetError());
    return false;
  }
  uint32_t at = 0;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.Stands()) { continue; }
    if (one.Writes != nullptr) {
      one.Writes(one.Carrying,
                 reinterpret_cast<float *>(mapped + at),
                 one.Bytes / static_cast<uint32_t>(sizeof(float)));
    } else {
      std::memcpy(mapped + at, one.From, one.Bytes);
    }
    at = (at + one.Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device_, Bulk_.Get());

  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device_);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  at = 0;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.Stands()) { continue; }
    const SDL_GPUTransferBufferLocation source{.transfer_buffer = Bulk_.Get(), .offset = at};
    const SDL_GPUBufferRegion into{
        .buffer = Buffer(one.Which).Get(), .offset = one.Offset, .size = one.Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, false);
    at = (at + one.Bytes + 15u) & ~15u;
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  return true;
}

bool SubjectResidency::Grow(Stream which, Need need, std::string &error) {
  OwnedBuffer &held = Buffer(which);
  uint32_t *const stood = HeldAt(which);
  if (held && *stood >= need.Bytes) { return true; }
  uint32_t widened = *stood > 0 ? *stood : need.Bytes;
  while (widened < need.Bytes) { widened *= 2u; }
  SDL_GPUBufferCreateInfo wanted{};
  wanted.usage = need.Usage;
  wanted.size = widened;
  OwnedBuffer fresh(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
  gBuffersMade.fetch_add(1u, std::memory_order_relaxed);
  if (!fresh) {
    error = std::format(Says::kStreamFoundNoRoom, SDL_GetError());
    return false;
  }
  if (held && *stood > 0) {
    SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device_);
    SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
    const SDL_GPUBufferLocation from{.buffer = held.Get(), .offset = 0};
    const SDL_GPUBufferLocation to{.buffer = fresh.Get(), .offset = 0};
    SDL_CopyGPUBufferToBuffer(copy, &from, &to, *stood, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(commands);
  }
  held = std::move(fresh);
  *stood = widened;
  return true;
}

void SubjectResidency::FlushCrossings(SDL_GPUCommandBuffer *commands) {
  if (StagedCount_ == 0 || commands == nullptr) { return; }
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  for (size_t at = 0; at < StagedCount_; ++at) {
    const SDL_GPUTransferBufferLocation source{.transfer_buffer = Staged_[at].Staging,
                                               .offset = Staged_[at].From};
    const SDL_GPUBufferRegion into{
        .buffer = Staged_[at].Into, .offset = Staged_[at].Offset, .size = Staged_[at].Bytes};
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
  static const std::array<uint8_t, 4> white = {{255, 255, 255, 255}};
  const uint32_t width = texture.Width > 0 ? texture.Width : 1;
  const uint32_t height = texture.Height > 0 ? texture.Height : 1;
  const uint8_t *texels = (texture.Rgba != nullptr) ? texture.Rgba : white.data();
  std::vector<float> linear(static_cast<size_t>(width) * height * 4u, 0.0f);
  for (size_t texel = 0; texel < linear.size() / 4u; ++texel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      const uint8_t code = texels[texel * 4u + channel];
      linear[texel * 4u + channel] =
          decode == Transfer::Srgb ? LinearFromSrgb8(code) : static_cast<float>(code) / kByteSteps;
    }
    linear[texel * kRgbaChannels + kAlphaChannel] =
        static_cast<float>(texels[texel * kRgbaChannels + kAlphaChannel]) / kByteSteps;
  }

  if (kind == TexelKind::Direction) {
    for (size_t texel = 0; texel < linear.size() / 4u; ++texel) {
      linear[texel * kRgbaChannels + kAlphaChannel] = 1.0f;
    }
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
  bound.Image = OwnedTexture(Device_, SDL_CreateGPUTexture(Device_, &wantedTexture));

  const uint32_t indexChannels = kind == TexelKind::Direction ? 0u : IndexChannelsOf(linear);
  std::vector<float> level = linear;
  uint32_t levelWidth = width;
  uint32_t levelHeight = height;
  for (uint32_t which = 0; which < levels; ++which) {
    if (which > 0) {
      std::vector<float> smaller;
      const Texels made = HalveInPlace(
          level, {.WidthPx = levelWidth, .HeightPx = levelHeight}, smaller, kind, indexChannels);
      level.swap(smaller);
      levelWidth = made.WidthPx;
      levelHeight = made.HeightPx;
    }
    const uint32_t bytes = levelWidth * levelHeight * 4u * static_cast<uint32_t>(sizeof(float));
    SDL_GPUTransferBufferCreateInfo wantedTransfer{};
    wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    wantedTransfer.size = bytes;
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(Device_, &wantedTransfer);
    gStagingMade.fetch_add(1u, std::memory_order_relaxed);
    gUploads.fetch_add(1u, std::memory_order_relaxed);
    gUploadsEver.fetch_add(1u, std::memory_order_relaxed);
    gUploadBytes.fetch_add(bytes, std::memory_order_relaxed);
    void *const mappedLevel = SDL_MapGPUTransferBuffer(Device_, staging, false);
    if (mappedLevel == nullptr) {
      SDL_ReleaseGPUTransferBuffer(Device_, staging);
      bound.Image.Reset();
      return bound;
    }
    std::memcpy(mappedLevel, level.data(), bytes);
    SDL_UnmapGPUTransferBuffer(Device_, staging);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_);
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
    SDL_ReleaseGPUTransferBuffer(Device_, staging);
  }

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.address_mode_u = AddressOf(texture.WrapU);
  wantedSampler.address_mode_v = AddressOf(texture.WrapV);
  wantedSampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  wantedSampler.min_filter = FilterOf(texture.Minify);
  wantedSampler.mag_filter = FilterOf(texture.Magnify);

  wantedSampler.mipmap_mode = texture.Mip == SubjectMip::Nearest ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                                                 : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

  wantedSampler.max_lod = kEveryMip;
  bound.Sample = OwnedSampler(Device_, SDL_CreateGPUSampler(Device_, &wantedSampler));
  return bound;
}

} // namespace outshine::Render
