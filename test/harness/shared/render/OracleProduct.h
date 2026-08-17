/* WHAT THE PREPARER CALLED THE FILE, COMPOSED ONCE ON THIS SIDE OF THE BOUNDARY.
 *
 * `test/outshine/corpus/prep/manifest.py:output_names_for` is the declaration and this is the reader's copy
 * of the same rule. The copy exists because the two sides are two languages; what it must not be is
 * four copies, and it was: `oracle.exr` was spelled in the missing-input check and in the oracle
 * read, `oracle.seed-shift.exr` in a third place and `oracle.normal.raw` in a fourth. A frame index
 * in the name (board:1169) is exactly the change that would have found three of those four late.
 *
 * A FRAME IS ABSENT OR IT IS A NUMBER, which is what `std::optional` says and a sentinel does not:
 * a still case names no frame at all and keeps the names the corpus already carries. */
#ifndef RENDER_ORACLE_PRODUCT_H
#define RENDER_ORACLE_PRODUCT_H

#include <cstdio>
#include <optional>
#include <string>

namespace outshine::Render::Parity {

struct OracleProduct {
  /* Empty for the picture itself; otherwise the preparer's own name for a render pass --
   * `normal`, `materialIndex`, `objectIndex`, `uv`. */
  std::string Quantity;
  /* The recipe. `default` is the one the acceptance numbers are judged on and carries no suffix. */
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

} // namespace outshine::Render::Parity

#endif
