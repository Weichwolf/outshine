#include "R8FieldTexture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;

std::vector<uint8_t> Pattern(uint32_t side, uint32_t tag) {
  std::vector<uint8_t> values(size_t{side} * side);
  for (uint32_t row = 0; row < side; ++row) {
    for (uint32_t col = 0; col < side; ++col) {
      values[size_t{row} * side + col] = static_cast<uint8_t>(row * 47 + col * 13 + tag);
    }
  }
  return values;
}

void UploadAndRetire(uint32_t baseSide, uint32_t mipCount) {
  auto device = CreateDevice({.Validation = true});
  CHECK(device, "a real native GPU device opens with validation");
  if (!device) { return; }
  auto commands = (*device)->Begin();
  CHECK(commands, "the R8 upload batch opens");
  if (!commands) { return; }
  std::vector<std::vector<uint8_t>> expected(mipCount);
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    expected[mip] = Pattern(baseSide >> mip, mip * 154 + 19);
  }
  auto input = expected;
  std::vector<R8FieldMip> mips(mipCount);
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    mips[mip] = {.Side = baseSide >> mip, .Pixels = input[mip]};
  }
  const auto bytes = R8FieldTextureStorage(mips);
  CHECK(bytes, "the selected coarse mip storage is planned before allocation");
  if (!bytes) { return; }
  CHECK(!UploadR8FieldTexture(
            **device,
            **commands,
            mips,
            {.TexelBytes = bytes->TexelBytes - 1, .TransferBytes = bytes->TransferBytes}),
        "insufficient resident texel admission refuses the upload");
  CHECK(!UploadR8FieldTexture(
            **device,
            **commands,
            mips,
            {.TexelBytes = bytes->TexelBytes, .TransferBytes = bytes->TransferBytes - 1}),
        "insufficient transient transfer admission refuses the upload");
  auto field = UploadR8FieldTexture(**device, **commands, mips, *bytes);
  CHECK(field, "the bounded native field texture uploads");
  if (!field) { return; }
  const auto &description = field->Image->Description();
  CHECK(description.Width == baseSide && description.Height == baseSide &&
            description.Mips == mipCount && description.Texels == Format::R8Unorm &&
            field->Storage.TexelBytes == bytes->TexelBytes,
        "native allocation contains only the supplied mip extents");
  for (auto &pixels : input) { std::vector<uint8_t>().swap(pixels); }
  auto output = (*device)->CreateBuffer({.Bytes = bytes->TransferBytes,
                                         .Use = BufferUse::CopyDestination,
                                         .Placement = Memory::Readback});
  CHECK(output, "aligned native readback storage allocates");
  if (!output) { return; }
  uint64_t offset = 0;
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    const uint32_t side = baseSide >> mip;
    CHECK((*commands)->Copy(*field->Image,
                            **output,
                            {.BufferOffset = offset,
                             .RowBytes = 256,
                             .Region = {.Mip = mip, .Width = side, .Height = side}}),
          "every supplied mip records an independent readback");
    offset += uint64_t{256} * side;
  }
  field->Image.reset();
  field->Sampling.reset();
  const auto completed = (*device)->Submit(std::move(*commands));
  CHECK(completed, "native commands retain destroyed upload/image wrappers through submission");
  if (!completed) { return; }
  const auto ready = (*device)->Wait(*completed, std::chrono::seconds(5));
  CHECK(ready && *ready == CompletionStatus::Ready,
        "the upload and readback complete within a deadline");
  if (!ready || *ready != CompletionStatus::Ready) { return; }
  const auto mapped = (*device)->MapRead(**output, {.Bytes = bytes->TransferBytes});
  CHECK(mapped, "completed mip pixels can be inspected");
  if (!mapped) { return; }
  offset = 0;
  for (uint32_t mip = 0; mip < mipCount; ++mip) {
    const uint32_t side = baseSide >> mip;
    bool same = true;
    for (uint32_t row = 0; row < side; ++row) {
      const auto got = mapped->subspan(offset + uint64_t{256} * row, side);
      const auto wanted = std::as_bytes(std::span(expected[mip])).subspan(size_t{row} * side, side);
      same = same && std::ranges::equal(got, wanted);
    }
    CHECK(same, "submitted mip pixels survive source and native wrapper destruction unchanged");
    offset += uint64_t{256} * side;
  }
  CHECK((*device)->Unmap(**output), "native readback ownership is released");
}
}

int main() {
  UploadAndRetire(32, 2);
  UploadAndRetire(66, 1);
  UploadAndRetire(130, 1);
  return Report();
}
