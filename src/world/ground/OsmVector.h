#ifndef OUTSHINE_WORLD_GROUND_OSMVECTOR_H
#define OUTSHINE_WORLD_GROUND_OSMVECTOR_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Ground {

class OsmVector {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;
    bool Exterior = true;
  };

  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    uint32_t FirstTag = 0, TagCount = 0;
    int Type = 0;
  };

  [[nodiscard]] bool
  Parse(const uint8_t *bytes, size_t len, const char *layer, bool *present = nullptr);

  [[nodiscard]] int Extent() const { return Extent_; }

  [[nodiscard]] const std::vector<Feature> &Features() const { return Features_; }

  [[nodiscard]] const std::vector<Ring> &Rings() const { return Rings_; }

  [[nodiscard]] const std::vector<int32_t> &Points() const { return Points_; }

  double Num(const Feature &f, const char *key, double def) const;
  std::string_view Str(const Feature &f, const char *key) const;

  struct Tag {
    std::string_view Key, Str;
    double Num = 0.0;
    bool IsNum = false;
  };

  [[nodiscard]] static uint32_t TagCount(const Feature &f) { return f.TagCount / 2; }

  [[nodiscard]] Tag TagAt(const Feature &f, uint32_t i) const;

private:
  int Extent_ = 4096;
  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<int32_t> Points_;
  std::vector<uint32_t> Tags_;
  std::vector<std::string> Keys_;
  std::vector<double> Values_;
  std::vector<std::string> ValueStrs_;
  std::vector<bool> ValueIsNum_;
};

} // namespace outshine::Ground
#endif
