#include "Png.h"

#include <cstring>

#include <zlib.h>

namespace outshine::Io {

namespace {

constexpr uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
constexpr size_t kMaxSide = 16384;

uint32_t Big(const uint8_t *at) {
  return (static_cast<uint32_t>(at[0]) << 24) | (static_cast<uint32_t>(at[1]) << 16) |
         (static_cast<uint32_t>(at[2]) << 8) | static_cast<uint32_t>(at[3]);
}

int Paeth(int left, int above, int corner) {
  const int guess = left + above - corner;
  const int toLeft = guess > left ? guess - left : left - guess;
  const int toAbove = guess > above ? guess - above : above - guess;
  const int toCorner = guess > corner ? guess - corner : corner - guess;
  if (toLeft <= toAbove && toLeft <= toCorner) { return left; }
  return toAbove <= toCorner ? above : corner;
}

Png Refuse(std::string why) {
  Png out;
  out.Error = std::move(why);
  return out;
}

} // namespace

Png ReadPng(const uint8_t *bytes, size_t length) {
  if (bytes == nullptr || length < sizeof(kSignature) + 12) {
    return Refuse("a PNG is at least a signature and one chunk, and this is " +
                  std::to_string(length) + " bytes");
  }
  if (std::memcmp(bytes, kSignature, sizeof(kSignature)) != 0) {
    return Refuse("these bytes do not begin with the PNG signature, so they are not a PNG");
  }

  uint32_t wide = 0, high = 0;
  uint8_t depth = 0, colour = 0, interlace = 0;
  bool haveHead = false;
  std::vector<uint8_t> squeezed;

  size_t at = sizeof(kSignature);
  while (at + 8 <= length) {
    const uint32_t size = Big(bytes + at);
    const uint8_t *name = bytes + at + 4;
    const size_t from = at + 8;
    if (from + static_cast<size_t>(size) + 4 > length) {
      return Refuse("a chunk at byte " + std::to_string(at) + " claims " + std::to_string(size) +
                    " bytes and the file has " + std::to_string(length - from) + " left");
    }

    if (std::memcmp(name, "IHDR", 4) == 0) {
      if (size != 13) {
        return Refuse("an IHDR is 13 bytes and this one is " + std::to_string(size));
      }
      wide = Big(bytes + from);
      high = Big(bytes + from + 4);
      depth = bytes[from + 8];
      colour = bytes[from + 9];
      interlace = bytes[from + 12];
      haveHead = true;
    } else if (std::memcmp(name, "IDAT", 4) == 0) {
      squeezed.insert(squeezed.end(), bytes + from, bytes + from + size);
    } else if (std::memcmp(name, "IEND", 4) == 0) {
      break;
    }
    at = from + static_cast<size_t>(size) + 4;
  }

  if (!haveHead) { return Refuse("this PNG carries no IHDR, so it never said how big it is"); }
  if (wide == 0 || high == 0 || wide > kMaxSide || high > kMaxSide) {
    return Refuse("a PNG of " + std::to_string(wide) + " by " + std::to_string(high) +
                  " reaches the bound of " + std::to_string(kMaxSide));
  }
  if (depth != 8) {
    return Refuse("this reader takes 8 bits a channel and this PNG carries " +
                  std::to_string(static_cast<int>(depth)) +
                  " -- an elevation tile is 8-bit RGB and anything else is a source we did not "
                  "declare");
  }
  if (colour != 2 && colour != 6) {
    return Refuse("this reader takes truecolour, with or without alpha, and this PNG is colour "
                  "type " +
                  std::to_string(static_cast<int>(colour)));
  }
  if (interlace != 0) {
    return Refuse("this reader takes no interlaced PNG, and this one is interlaced");
  }
  if (squeezed.empty()) { return Refuse("this PNG carries no IDAT, so it has no pixels"); }

  const uint32_t channels = colour == 2 ? 3u : 4u;
  const size_t stride = static_cast<size_t>(wide) * channels;
  const size_t wanted = (stride + 1) * static_cast<size_t>(high);

  std::vector<uint8_t> raw(wanted);
  uLongf got = static_cast<uLongf>(wanted);
  const int how =
      uncompress(raw.data(), &got, squeezed.data(), static_cast<uLong>(squeezed.size()));
  if (how != Z_OK || static_cast<size_t>(got) != wanted) {
    return Refuse("the IDAT inflated to " + std::to_string(static_cast<size_t>(got)) +
                  " bytes where " + std::to_string(wanted) + " were needed, and zlib said " +
                  std::to_string(how));
  }

  Png out;
  out.Wide = wide;
  out.High = high;
  out.Channels = channels;
  out.Bytes.assign(static_cast<size_t>(high) * stride, 0);

  for (uint32_t row = 0; row < high; ++row) {
    const uint8_t filter = raw[(stride + 1) * static_cast<size_t>(row)];
    const uint8_t *in = raw.data() + (stride + 1) * static_cast<size_t>(row) + 1;
    uint8_t *outRow = out.Bytes.data() + stride * static_cast<size_t>(row);
    const uint8_t *above =
        row > 0 ? out.Bytes.data() + stride * static_cast<size_t>(row - 1) : nullptr;

    for (size_t byte = 0; byte < stride; ++byte) {
      const int left = byte >= channels ? outRow[byte - channels] : 0;
      const int up = above != nullptr ? above[byte] : 0;
      const int corner = (above != nullptr && byte >= channels) ? above[byte - channels] : 0;
      int value = in[byte];
      switch (filter) {
        case 0: break;
        case 1: value += left; break;
        case 2: value += up; break;
        case 3: value += (left + up) / 2; break;
        case 4: value += Paeth(left, up, corner); break;
        default:
          return Refuse("row " + std::to_string(row) + " declares filter " +
                        std::to_string(static_cast<int>(filter)) + ", and PNG has five");
      }
      outRow[byte] = static_cast<uint8_t>(value & 0xff);
    }
  }

  out.Read = true;
  return out;
}

} // namespace outshine::Io
