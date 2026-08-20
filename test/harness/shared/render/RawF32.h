#ifndef RENDER_RAWF32_H
#define RENDER_RAWF32_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Exr.h"

namespace outshine::Render::Parity {

class RawF32 {
public:
  [[nodiscard]] bool ReadFile(const std::string &path) {
    Error_.clear();
    Samples_.clear();
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) { return Refuse(path + ": no oracle to compare against"); }
    std::vector<uint8_t> bytes;
    uint8_t block[1 << 16];
    for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
         read = std::fread(block, 1, sizeof block, file)) {
      bytes.insert(bytes.end(), block, block + read);
    }
    std::fclose(file);
    return Read(bytes, path);
  }

  [[nodiscard]] bool Read(const std::vector<uint8_t> &bytes, const std::string &path) {
    Error_.clear();
    Samples_.clear();
    if (bytes.size() < 36) {
      return Refuse(path + ": " + std::to_string(bytes.size()) +
                    " bytes is shorter than the 36-byte fixed header");
    }
    if (std::memcmp(bytes.data(), "OSRAWF32", 8) != 0) {
      return Refuse(path + ": the first eight bytes are not OSRAWF32");
    }
    uint32_t field[7];
    std::memcpy(field, bytes.data() + 8, sizeof field);

    if (field[0] != 0x01020304u) {
      return Refuse(path + ": byte-order mark reads " + std::to_string(field[0]) +
                    ", so the dump was written on the other endianness");
    }
    if (field[1] != 1u) {
      return Refuse(path + ": raw version " + std::to_string(field[1]) + ", and this reads 1");
    }
    Width_ = (int)field[2];
    Height_ = (int)field[3];
    Channels_ = (int)field[4];
    TopRowFirst_ = field[6] == 0u;
    const size_t header = field[5];
    if (Width_ <= 0 || Height_ <= 0 || Channels_ <= 0) {
      return Refuse(path + ": declares " + std::to_string(Width_) + "x" + std::to_string(Height_) +
                    " with " + std::to_string(Channels_) + " channels");
    }
    const size_t samples = (size_t)Width_ * (size_t)Height_ * (size_t)Channels_;
    if (bytes.size() != header + samples * sizeof(float)) {
      return Refuse(path + ": " + std::to_string(bytes.size()) + " bytes, and its own header says " +
                    std::to_string(header + samples * sizeof(float)));
    }
    Samples_.resize(samples);
    std::memcpy(Samples_.data(), bytes.data() + header, samples * sizeof(float));
    return true;
  }

  [[nodiscard]] bool ReadExrFile(const std::string &path) {
    Error_.clear();
    Samples_.clear();
    Exr exr;
    if (!exr.ReadFile(path)) { return Refuse(exr.Error()); }
    static const char *const kRgba[4] = {"R", "G", "B", "A"};
    const std::vector<float> *plane[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int channel = 0; channel < 4; ++channel) {
      plane[channel] = exr.Plane(kRgba[channel]);
      if (!plane[channel]) {
        return Refuse(path + ": no channel named " + kRgba[channel] + " in the oracle's EXR");
      }
    }
    Width_ = exr.Width();
    Height_ = exr.Height();
    Channels_ = 4;
    TopRowFirst_ = true;
    const size_t pixels = (size_t)Width_ * (size_t)Height_;
    Samples_.resize(pixels * 4u);
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
      for (int channel = 0; channel < 4; ++channel) {
        Samples_[pixel * 4u + (size_t)channel] = (*plane[channel])[pixel];
      }
    }
    return true;
  }

  const std::string &Error() const { return Error_; }
  int Width() const { return Width_; }
  int Height() const { return Height_; }
  int Channels() const { return Channels_; }
  bool TopRowFirst() const { return TopRowFirst_; }

  float At(int x, int y, int channel) const {
    const size_t index =
        ((size_t)y * (size_t)Width_ + (size_t)x) * (size_t)Channels_ + (size_t)channel;
    return index < Samples_.size() ? Samples_[index] : 0.0f;
  }

private:
  [[nodiscard]] bool Refuse(const std::string &why) {
    Error_ = why;
    return false;
  }

  std::string Error_;
  std::vector<float> Samples_;
  int Width_ = 0, Height_ = 0, Channels_ = 0;
  bool TopRowFirst_ = true;
};

[[nodiscard]] inline bool WriteRawF32(const std::string &path, const std::vector<float> &samples,
                                      int width, int height, int channels, std::string &error) {
  if (samples.size() != (size_t)width * (size_t)height * (size_t)channels) {
    error = path + ": " + std::to_string(samples.size()) + " samples is not " +
            std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(channels);
    return false;
  }
  if (channels != 4) {
    error = path + ": this writer emits RGBA and was asked for " + std::to_string(channels) +
            " channels";
    return false;
  }

  static const char kNames[8] = {'R', 0, 'G', 0, 'B', 0, 'A', 0};
  constexpr uint32_t kHeaderBytes = 44;
  const uint32_t field[7] = {0x01020304u,        1u,           (uint32_t)width, (uint32_t)height,
                             (uint32_t)channels, kHeaderBytes, 0u};
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) {
    error = path + " could not be opened for writing";
    return false;
  }
  const bool whole = std::fwrite("OSRAWF32", 1, 8, file) == 8 &&
                     std::fwrite(field, 1, sizeof field, file) == sizeof field &&
                     std::fwrite(kNames, 1, sizeof kNames, file) == sizeof kNames &&
                     std::fwrite(samples.data(), sizeof(float), samples.size(), file) ==
                         samples.size();
  std::fclose(file);
  if (!whole) {
    error = path + " was opened and not written whole";
    return false;
  }
  return true;
}

}
#endif
