#include "RawSourceCache.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace outshine::Data {
namespace {
constexpr size_t kEncodingBytesMost = std::numeric_limits<size_t>::max();
constexpr std::string_view kSourceDomain = "source-declaration-2";

bool SourceFields(BinaryValueWriter &out, const SourceDecl &decl) {
  return out.Text(decl.Id) && out(decl.Version) && out.Text(decl.Revision) &&
         out.Text(Name(decl.Kind));
}

std::string Digest(const BinaryValueWriter &out) {
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}

bool SourceIdentityEncodable(const SourceDecl &decl) noexcept {
  constexpr auto lengthMost = std::numeric_limits<uint32_t>::max();
  return Name(decl.Kind)[0] != '\0' && Name(decl.Wire)[0] != '\0' && decl.Id.size() <= lengthMost &&
         decl.Revision.size() <= lengthMost && decl.Endpoint.size() <= lengthMost &&
         decl.PayloadSha256.size() <= lengthMost && decl.Schema.size() <= lengthMost;
}

std::string ContentKey(const SourceDecl &decl, const Address &at) {
  if (!SourceIdentityEncodable(decl)) { return {}; }
  BinaryValueWriter out(kEncodingBytesMost);
  if (!out.Text("raw-source-content-2") || !SourceFields(out, decl) || !out.Text(at.Text()) ||
      !out.Text(decl.Endpoint) || !out.Text(decl.PayloadSha256)) {
    return {};
  }
  return Digest(out);
}

std::string ContentKey(const SourceDecl &decl,
                       const Address &at,
                       const std::optional<ByteRange> &range,
                       std::string_view entityTag) {
  auto object = ContentKey(decl, at);
  if (object.empty() || !range) { return object; }
  BinaryValueWriter out(kEncodingBytesMost);
  if (!out.Text("raw-source-range-2") || !out.Text(object) || !out(range->First, range->Length) ||
      !out.Text(entityTag)) {
    return {};
  }
  return Digest(out);
}

std::string SourceKey(const SourceDecl &decl) {
  if (!SourceIdentityEncodable(decl)) { return {}; }
  const std::string_view kind(Name(decl.Kind));
  const std::string_view wire(Name(decl.Wire));
  const uint64_t bytes = uint64_t{8} * sizeof(uint32_t) + kSourceDomain.size() + decl.Id.size() +
                         decl.Revision.size() + kind.size() + wire.size() + decl.Endpoint.size() +
                         decl.PayloadSha256.size();
  if (bytes > kEncodingBytesMost) { return {}; }
  BinaryValueWriter out(kEncodingBytesMost);
  out.Out.Reserve(static_cast<size_t>(bytes));
  if (!out.Text(kSourceDomain) || !SourceFields(out, decl) || !out.Text(wire) ||
      !out.Text(decl.Endpoint) || !out.Text(decl.PayloadSha256)) {
    return {};
  }
  return Digest(out);
}
}
