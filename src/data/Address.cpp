#include "Address.h"

#include <cstdio>

namespace outshine::Data {

std::string Address::Text() const {
  char text[48];
  if (How_ == Scheme::TileZxy) std::snprintf(text, sizeof text, "%d/%u/%u", Z_, X_, Y_);
  else std::snprintf(text, sizeof text, "w/%u", X_);
  return std::string(text);
}

} // namespace outshine::Data
