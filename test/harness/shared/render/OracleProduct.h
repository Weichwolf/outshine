#ifndef RENDER_ORACLE_PRODUCT_H
#define RENDER_ORACLE_PRODUCT_H

#include <cstdio>
#include <optional>
#include <string>

namespace outshine::Render::Parity {

struct OracleProduct {

  std::string Quantity;

  std::string Recipe = "default";
  std::optional<int> Frame;

  [[nodiscard]] std::string Name(const char *extension) const {
    std::string name = "oracle";
    if (!Quantity.empty()) { name += "." + Quantity; }
    if (Recipe != "default") { name += "." + Recipe; }
    if (Frame.has_value()) {
      char digits[16];
      std::snprintf(digits, sizeof digits, ".f%04d", *Frame);
      name += digits;
    }
    return name + extension;
  }

  [[nodiscard]] std::string Exr() const { return Name(".exr"); }
  [[nodiscard]] std::string Raw() const { return Name(".raw"); }
};

}

#endif
