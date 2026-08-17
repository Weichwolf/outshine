/* A SCANLINE OpenEXR READER IN C++ (board:1119), and `test/harness/shared/corpus/prep/exr.py` is its specification.
 *
 * WHY IT EXISTS. `oracle.raw` exists only because C++ could not read an EXR: it is a flat f32 dump of
 * what `oracle.exr` already holds, 265 MB against 16 MB per case, a 16x DECOMPRESSED CACHE of a file
 * the corpus keeps anyway. With this reader the raw is derivable rather than stored, and the corpus
 * retention rule stops costing a Blender re-render per suite run.
 *
 * WHY NOT A PACKAGE. `OpenEXR` is a dependency for a format that is a header, an offset table and
 * zlib -- and zlib is already in this process, linked through SDL3_image's libpng. `CLAUDE.md`'s rule
 * is satisfied either way: a package the host provides, or ours.
 *
 * WHAT IT READS AND WHAT IT REFUSES. Scanline images, `NONE`/`ZIPS`/`ZIP`, `HALF` and `FLOAT`
 * channels. TILED, DEEP AND MULTIPART ARE REFUSED BY NAME rather than half-read -- the same three the
 * Python reader refuses and for the same reason: this serves one producer, and a file it does not
 * understand is a defect in that producer rather than an input to accommodate.
 *
 * IT IS A HEADER BECAUSE ITS ONE CONSUMER IS A TEST and `RawF32.h` beside it is one too; a reader
 * whose whole surface is "open this file, hand me the planes" has no state worth a translation unit. */
#ifndef RENDER_EXR_H
#define RENDER_EXR_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <zlib.h>

namespace outshine::Render::Parity {

class Exr {
public:
  [[nodiscard]] bool ReadFile(const std::string &path) {
    Error_.clear();
    Planes_.clear();
    Width_ = Height_ = 0;
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) { return Refuse(path + ": no such file to read as an OpenEXR image"); }
    std::vector<uint8_t> bytes;
    uint8_t block[1 << 16];
    for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
         read = std::fread(block, 1, sizeof block, file)) {
      bytes.insert(bytes.end(), block, block + read);
    }
    std::fclose(file);
    return Read(bytes, path);
  }

  [[nodiscard]] const std::string &Error() const { return Error_; }
  [[nodiscard]] int Width() const { return Width_; }
  [[nodiscard]] int Height() const { return Height_; }

  /* THE CHANNEL'S NAME KEEPS ANY LAYER PREFIX THE PRODUCER WROTE -- `normal.R` and `R` are different
   * channels and stripping the prefix here would merge two layers that differ only by it. */
  [[nodiscard]] const std::vector<float> *Plane(const std::string &channel) const {
    const auto found = Planes_.find(channel);
    return found == Planes_.end() ? nullptr : &found->second;
  }

  [[nodiscard]] std::vector<std::string> Channels() const {
    std::vector<std::string> names;
    names.reserve(Planes_.size());
    for (const auto &plane : Planes_) { names.push_back(plane.first); }
    return names;
  }

private:
  [[nodiscard]] bool Refuse(const std::string &why) {
    Error_ = why;
    return false;
  }

  struct Channel {
    std::string Name;
    int PixelType = 0; /* 0 UINT, 1 HALF, 2 FLOAT */
  };

  /* binary16 to f32 is exact, so widening here loses nothing and one currency leaves this reader. */
  [[nodiscard]] static float HalfToFloat(uint16_t bits) {
    const uint32_t sign = (uint32_t)(bits >> 15) << 31;
    uint32_t exponent = (bits >> 10) & 0x1Fu;
    uint32_t mantissa = bits & 0x3FFu;
    uint32_t wide = 0;
    if (exponent == 0) {
      if (mantissa != 0) {
        /* Subnormal: renormalise into the f32 exponent range rather than truncating to zero. */
        int shift = 0;
        while ((mantissa & 0x400u) == 0) {
          mantissa <<= 1;
          ++shift;
        }
        mantissa &= 0x3FFu;
        wide = sign | ((uint32_t)(127 - 15 - shift) << 23) | (mantissa << 13);
      } else {
        wide = sign;
      }
    } else if (exponent == 0x1Fu) {
      wide = sign | 0x7F800000u | (mantissa << 13);
    } else {
      wide = sign | ((exponent + 127u - 15u) << 23) | (mantissa << 13);
    }
    float out = 0.0f;
    std::memcpy(&out, &wide, sizeof out);
    return out;
  }

  /* zlib, THEN EXR's OWN BYTE PREDICTOR AND ITS TWO-HALF INTERLEAVE, in that order. Neither step is
   * zlib's and neither is optional: the predictor makes the stream compressible and the interleave
   * separates a float's high bytes from its low ones. */
  [[nodiscard]] bool Unzip(const uint8_t *from, size_t size, std::vector<uint8_t> &into) {
    uLongf produced = (uLongf)into.size();
    if (uncompress(into.data(), &produced, from, (uLong)size) != Z_OK) { return false; }
    into.resize(produced);
    int previous = (int)into.empty() ? 0 : 0;
    for (size_t at = 1; at < into.size(); ++at) {
      previous = (int)into[at - 1] + (int)into[at] - 128;
      into[at] = (uint8_t)previous;
    }
    std::vector<uint8_t> ordered(into.size());
    const size_t split = (into.size() + 1) / 2;
    size_t low = 0, high = split;
    for (size_t at = 0; at < ordered.size(); ++at) {
      ordered[at] = (at % 2 == 0) ? into[low++] : into[high++];
    }
    into.swap(ordered);
    return true;
  }

  [[nodiscard]] bool Read(const std::vector<uint8_t> &data, const std::string &path) {
    constexpr uint32_t kMagic = 0x01312F76u;
    if (data.size() < 8) { return Refuse(path + ": shorter than an OpenEXR magic and version"); }
    uint32_t magic = 0, flags = 0;
    std::memcpy(&magic, data.data(), sizeof magic);
    std::memcpy(&flags, data.data() + 4, sizeof flags);
    if (magic != kMagic) { return Refuse(path + ": not an OpenEXR file"); }
    if ((flags >> 9) & 1u) {
      return Refuse(path + ": tiled, and this reader serves scanline images");
    }
    if (((flags >> 11) & 1u) || ((flags >> 12) & 1u)) {
      return Refuse(path + ": deep or multipart, and this reader serves neither");
    }

    size_t at = 8;
    std::vector<Channel> channels;
    int dataWindow[4] = {0, 0, 0, 0};
    int compression = -1;
    bool haveChannels = false, haveWindow = false;
    while (true) {
      const size_t nameEnd = Find(data, at);
      if (nameEnd == std::string::npos) { return Refuse(path + ": the header does not terminate"); }
      const std::string name((const char *)data.data() + at, nameEnd - at);
      at = nameEnd + 1;
      if (name.empty()) { break; }
      const size_t kindEnd = Find(data, at);
      if (kindEnd == std::string::npos) { return Refuse(path + ": an attribute has no type"); }
      at = kindEnd + 1;
      if (at + 4 > data.size()) { return Refuse(path + ": an attribute has no size"); }
      int32_t size = 0;
      std::memcpy(&size, data.data() + at, sizeof size);
      at += 4;
      if (size < 0 || at + (size_t)size > data.size()) {
        return Refuse(path + ": attribute '" + name + "' runs past the end of the file");
      }
      if (name == "channels") {
        if (!ReadChannels(data, at, (size_t)size, channels, path)) { return false; }
        haveChannels = true;
      } else if (name == "dataWindow" && size == 16) {
        std::memcpy(dataWindow, data.data() + at, sizeof dataWindow);
        haveWindow = true;
      } else if (name == "compression" && size == 1) {
        compression = (int)data[at];
      }
      at += (size_t)size;
    }
    if (!haveChannels) { return Refuse(path + ": the header declares no channels"); }
    if (!haveWindow) { return Refuse(path + ": the header declares no dataWindow"); }

    size_t perBlock = 0;
    switch (compression) {
      case 0: perBlock = 1; break; /* NONE */
      case 2: perBlock = 1; break; /* ZIPS */
      case 3: perBlock = 16; break; /* ZIP */
      default:
        return Refuse(path + ": compression " + std::to_string(compression) +
                      ", and this reader knows NONE, ZIPS and ZIP");
    }

    Width_ = dataWindow[2] - dataWindow[0] + 1;
    Height_ = dataWindow[3] - dataWindow[1] + 1;
    if (Width_ <= 0 || Height_ <= 0) {
      return Refuse(path + ": the dataWindow encloses no pixels");
    }
    const size_t pixels = (size_t)Width_ * (size_t)Height_;
    for (const Channel &channel : channels) { Planes_[channel.Name].assign(pixels, 0.0f); }

    const size_t blocks = ((size_t)Height_ + perBlock - 1) / perBlock;
    if (at + blocks * 8u > data.size()) {
      return Refuse(path + ": the scanline offset table runs past the end of the file");
    }
    std::vector<uint64_t> offsets(blocks);
    std::memcpy(offsets.data(), data.data() + at, blocks * sizeof(uint64_t));

    std::vector<uint8_t> scratch;
    for (const uint64_t offset : offsets) {
      if (offset + 8u > data.size()) { return Refuse(path + ": a scanline block is out of range"); }
      int32_t line = 0, size = 0;
      std::memcpy(&line, data.data() + offset, sizeof line);
      std::memcpy(&size, data.data() + offset + 4, sizeof size);
      if (size < 0 || offset + 8u + (size_t)size > data.size()) {
        return Refuse(path + ": a scanline block declares a length past the end of the file");
      }
      const size_t rows =
          (size_t)((dataWindow[3] - line + 1) < (int)perBlock ? (dataWindow[3] - line + 1)
                                                              : (int)perBlock);
      size_t stride = 0;
      for (const Channel &channel : channels) {
        stride += (size_t)Width_ * (channel.PixelType == 1 ? 2u : 4u);
      }
      const uint8_t *payload = data.data() + offset + 8u;
      if (compression != 0) {
        scratch.assign(rows * stride, 0);
        if (!Unzip(payload, (size_t)size, scratch)) {
          return Refuse(path + ": a scanline block did not inflate");
        }
        payload = scratch.data();
        if (scratch.size() < rows * stride) {
          return Refuse(path + ": a scanline block inflated short of its own row count");
        }
      } else if ((size_t)size < rows * stride) {
        return Refuse(path + ": an uncompressed scanline block is shorter than its rows");
      }

      /* WITHIN A BLOCK THE ROWS COME FIRST AND THE CHANNELS SECOND, in the header's own order -- so a
       * channel's row offset depends on every channel declared before it. */
      size_t cursor = 0;
      for (size_t row = 0; row < rows; ++row) {
        const size_t y = (size_t)(line - dataWindow[1]) + row;
        for (const Channel &channel : channels) {
          std::vector<float> &plane = Planes_[channel.Name];
          for (int x = 0; x < Width_; ++x) {
            const size_t into = y * (size_t)Width_ + (size_t)x;
            if (channel.PixelType == 1) {
              uint16_t bits = 0;
              std::memcpy(&bits, payload + cursor + (size_t)x * 2u, sizeof bits);
              plane[into] = HalfToFloat(bits);
            } else {
              float value = 0.0f;
              std::memcpy(&value, payload + cursor + (size_t)x * 4u, sizeof value);
              plane[into] = value;
            }
          }
          cursor += (size_t)Width_ * (channel.PixelType == 1 ? 2u : 4u);
        }
      }
    }
    return true;
  }

  [[nodiscard]] static size_t Find(const std::vector<uint8_t> &data, size_t from) {
    for (size_t at = from; at < data.size(); ++at) {
      if (data[at] == 0) { return at; }
    }
    return std::string::npos;
  }

  [[nodiscard]] bool ReadChannels(const std::vector<uint8_t> &data, size_t at, size_t size,
                                  std::vector<Channel> &out, const std::string &path) {
    const size_t end = at + size;
    while (at < end && data[at] != 0) {
      const size_t nameEnd = Find(data, at);
      if (nameEnd == std::string::npos || nameEnd >= end) {
        return Refuse(path + ": a channel name is unterminated");
      }
      Channel channel;
      channel.Name.assign((const char *)data.data() + at, nameEnd - at);
      at = nameEnd + 1;
      if (at + 16u > end) { return Refuse(path + ": a channel declaration is truncated"); }
      int32_t pixelType = 0;
      std::memcpy(&pixelType, data.data() + at, sizeof pixelType);
      if (pixelType != 1 && pixelType != 2) {
        return Refuse(path + ": channel '" + channel.Name + "' has pixel type " +
                      std::to_string(pixelType) + ", and this reader knows HALF and FLOAT");
      }
      channel.PixelType = pixelType;
      at += 16u; /* pixel type, pLinear + two pad bytes, xSampling, ySampling */
      out.push_back(channel);
    }
    return true;
  }

  std::string Error_;
  int Width_ = 0, Height_ = 0;
  std::map<std::string, std::vector<float>> Planes_;
};

} // namespace outshine::Render::Parity
#endif
