/* THE ORACLE'S PIXELS, as the preparer dumped them beside the EXR: SDL3 provides no EXR reader and
 * vendoring OpenEXR to compute a coverage fraction would buy nothing (doc/requirements.md I.26.10).
 *
 * AN ABSENT OR MALFORMED ORACLE IS A REFUSAL AND NEVER A SKIP. A case that quietly compares against
 * nothing is the hollow green the whole render suite is built to make impossible, so every early
 * return below carries the sentence that says which file and which field. */
#ifndef RENDER_ORACLERAW_H
#define RENDER_ORACLERAW_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace outshine::Render::Parity {

class OracleRaw {
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
    /* The writer put its own 0x01020304 down natively; a reader that sees it byte-swapped is on the
     * other endianness and would decode every float wrong without any length going out of range. */
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

} // namespace outshine::Render::Parity
#endif
