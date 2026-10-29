#ifdef __linux__
#include "GraphicsRecipe.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {
namespace {
class Encoding {
public:
  std::vector<std::byte> Bytes;

  void Number(uint64_t value) {
    const auto bytes = std::as_bytes(std::span(&value, 1));
    Bytes.insert(Bytes.end(), bytes.begin(), bytes.end());
  }

  void Blob(std::span<const std::byte> bytes) {
    Number(bytes.size());
    if (!bytes.empty()) { Bytes.insert(Bytes.end(), bytes.begin(), bytes.end()); }
  }

  void Shader(const ShaderCode &code) {
    const auto &[spirv, entry] = code;
    Blob(std::as_bytes(spirv));
    Blob(std::as_bytes(std::span(entry.data(), entry.size())));
  }

  void Stream(const VertexStream &stream) {
    const auto &[slot, stride, rate] = stream;
    Number(slot);
    Number(stride);
    Number(static_cast<uint64_t>(rate));
  }

  void Attribute(const VertexAttribute &attribute) {
    const auto &[location, stream, offset, format] = attribute;
    Number(location);
    Number(stream);
    Number(offset);
    Number(static_cast<uint64_t>(format));
  }

  void Blend(const BlendFunction &function) {
    const auto &[source, destination] = function;
    Number(static_cast<uint64_t>(source));
    Number(static_cast<uint64_t>(destination));
  }

  void Colour(const ColourTarget &colour) {
    const auto &[texels, write, blend, rgb, alpha] = colour;
    Number(static_cast<uint64_t>(texels));
    Number(static_cast<uint32_t>(write));
    Number(static_cast<uint64_t>(blend));
    Blend(rgb);
    Blend(alpha);
  }

  void Depth(const DepthTarget &depth) {
    const auto &[texels, compare, test, write] = depth;
    Number(static_cast<uint64_t>(texels));
    Number(static_cast<uint64_t>(compare));
    Number(static_cast<uint64_t>(test));
    Number(static_cast<uint64_t>(write));
  }
};
}

GraphicsRecipe GraphicsRecipe::From(const GraphicsDescription &description) {
  const auto
      &[vertex, fragment, streams, attributes, colours, depth, culling, front, clamp, label] =
          description;
  (void)label;
  Encoding encoding;
  const size_t scalarWords = 10 + (fragment ? 2 : 0) + (depth ? 4 : 0) + streams.size() * 3 +
                             attributes.size() * 4 + colours.size() * 7;
  encoding.Bytes.reserve(vertex.Spirv.size_bytes() + vertex.Entry.size() +
                         (fragment ? fragment->Spirv.size_bytes() + fragment->Entry.size() : 0) +
                         scalarWords * sizeof(uint64_t));
  encoding.Shader(vertex);
  encoding.Number(static_cast<uint64_t>(fragment.has_value()));
  if (fragment) { encoding.Shader(*fragment); }
  encoding.Number(streams.size());
  for (const auto &stream : streams) { encoding.Stream(stream); }
  encoding.Number(attributes.size());
  for (const auto &attribute : attributes) { encoding.Attribute(attribute); }
  encoding.Number(colours.size());
  for (const auto &colour : colours) { encoding.Colour(colour); }
  encoding.Number(static_cast<uint64_t>(depth.has_value()));
  if (depth) { encoding.Depth(*depth); }
  encoding.Number(static_cast<uint64_t>(culling));
  encoding.Number(static_cast<uint64_t>(front));
  encoding.Number(static_cast<uint64_t>(clamp));
  const std::string_view bytes(reinterpret_cast<const char *>(encoding.Bytes.data()),
                               encoding.Bytes.size());
  return {.Digest = static_cast<uint64_t>(std::hash<std::string_view>{}(bytes)),
          .Bytes = std::move(encoding.Bytes)};
}
}
#endif
