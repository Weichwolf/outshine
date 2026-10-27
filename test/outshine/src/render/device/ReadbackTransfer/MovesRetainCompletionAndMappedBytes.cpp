#include "ReadbackTransfer.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr std::array<uint32_t, 4> kWords{0x12345678u, 0x89ABCDEFu, 0x10203040u, 0x50607080u};
constexpr ByteRange kRange{.Offset = sizeof(uint32_t), .Bytes = 2 * sizeof(uint32_t)};

void Matches(std::span<const std::byte> bytes) {
  const auto expected = std::as_bytes(std::span(kWords).subspan(1, 2));
  CHECK(bytes.size() == expected.size() &&
            std::memcmp(bytes.data(), expected.data(), expected.size()) == 0,
        "readback retains the requested bytes after source release and ownership transfer");
}

ReadbackTransfer Transfer(Device &device, ByteRange range) {
  auto commands = Require(device.Begin());
  auto source = Upload(device, *commands, kWords);
  Require(device.Submit(std::move(commands)));
  auto transfer = Require(ReadbackTransfer::Start(device, *source, range));
  source.reset();
  return transfer;
}

void Ownership(Device &device) {
  auto original = Transfer(device, kRange);
  auto reader = std::move(original);
  original = Transfer(device, kRange);
  const auto reused = Require(original.Wait(device, std::chrono::seconds(5)));
  CHECK(reused.has_value(), "a moved-from source can receive and complete a new transfer");
  if (reused) { Matches(*reused); }
  CHECK(reader.RowBytes() == kRange.Bytes, "moving preserves the transfer extent");
  const auto first = Require(reader.Wait(device, std::chrono::seconds(5)));
  CHECK(first.has_value(), "moved transfer completes within its offline wait budget");
  if (!first) { return; }
  Matches(*first);
  const auto repeated = Require(reader.Poll(device));
  CHECK(repeated && repeated->data() == first->data(),
        "repeated polling reuses the owned mapping instead of attempting a second map");

  auto mapped = std::move(reader);
  const auto afterMove = Require(mapped.Wait(device, std::chrono::seconds(5)));
  CHECK(afterMove && afterMove->data() == first->data(),
        "moving a mapped transfer preserves its live bytes");
  if (afterMove) { Matches(*afterMove); }

  auto replaced = Transfer(device, {.Bytes = sizeof(uint32_t)});
  CHECK(Require(replaced.Wait(device, std::chrono::seconds(5))).has_value(),
        "move assignment replaces an already mapped transfer");
  replaced = std::move(mapped);
  const auto afterAssignment = Require(replaced.Poll(device));
  CHECK(afterAssignment && afterAssignment->data() == first->data() &&
            replaced.RowBytes() == kRange.Bytes,
        "move assignment transfers the mapping, completion and extent together");
  if (afterAssignment) { Matches(*afterAssignment); }
}

void Admission(Device &device, Device &foreign) {
  auto reader = Transfer(device, kRange);
  const auto rejected = reader.Wait(foreign, std::chrono::nanoseconds::zero());
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
        "a different device cannot consume the completion or map its buffer");
  bool threadRejected = false;
  std::thread worker([&] {
    const auto result = reader.Poll(device);
    threadRejected = !result && result.error().Kind == ErrorKind::Invalid;
  });
  worker.join();
  CHECK(threadRejected, "the device owner thread remains required after transfer creation");
  const auto retained = Require(reader.Wait(device, std::chrono::seconds(5)));
  CHECK(retained.has_value(), "rejected observers preserve the owner's pending transfer");
  if (retained) { Matches(*retained); }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  auto foreign = Require(CreateDevice({.Validation = true}));
  Ownership(*device);
  Admission(*device, *foreign);
  return Report();
}
