/* WHERE A CASE'S PRODUCTS GO: one directory, named once, and every product named relative to it.
 *
 * IT REPLACES AN INTERFACE OVER NOTHING. `src/clients/Artifacts.h` was an abstract class with three
 * writers, a `Settle()` and a three-valued `Delivery` -- and it existed because *"a directory
 * natively, an HTTP endpoint in the browser"*. The browser is gone, `FileArtifacts` went with it, and
 * at `a050a1d` the interface had ZERO implementations while two of its three states were
 * unproducible. `C.121`/`I.25`: an abstract interface over nothing is not an abstraction, it is a
 * shape waiting to be re-derived wrongly.
 *
 * AND IT LIVES UNDER `test/` BECAUSE THE LIBRARY DOES NOT WRITE ARTEFACTS. The only consumer this has
 * ever had is the render runner; a class in `src/` that no library code calls is the same defect one
 * directory further in.
 *
 * THE ROOT IS AN INVARIANT AND NOT A CONVENIENCE (`C.2`): a name that escapes it has no way to be
 * written, so "the runner wrote outside its case directory" is a refusal with a sentence rather than
 * a file somebody finds later. Two states, so the answer is a bool and a sentence -- the house error
 * channel -- rather than an enumeration whose second value means "no". */
#ifndef RENDER_PICTURES_H
#define RENDER_PICTURES_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Image.h"

namespace outshine::Render::Parity {

class Pictures {
public:
  explicit Pictures(std::string directory) : Directory_(std::move(directory)) {
    if (!Directory_.empty() && Directory_.back() != '/') { Directory_ += '/'; }
  }

  /* Encodes RGBA8, straight alpha, top row first -- the one convention both sides of a render case
   * state -- and writes it under `name`. Refuses a name that is empty, absolute or climbing. */
  [[nodiscard]] bool Png(const std::string &name, const std::vector<uint8_t> &rgba, int width,
                         int height, std::string &error) const {
    if (!Names(name, error)) { return false; }
    std::vector<uint8_t> encoded;
    if (!outshine::Clients::EncodePng(rgba.data(), width, height, encoded)) {
      error = name + " did not encode as a PNG at " + std::to_string(width) + "x" +
              std::to_string(height);
      return false;
    }
    std::FILE *file = std::fopen((Directory_ + name).c_str(), "wb");
    if (!file) {
      error = Directory_ + name + " could not be opened for writing";
      return false;
    }
    const bool whole = std::fwrite(encoded.data(), 1, encoded.size(), file) == encoded.size();
    std::fclose(file);
    if (!whole) {
      error = Directory_ + name + " was opened and not written whole";
      return false;
    }
    return true;
  }

private:
  [[nodiscard]] bool Names(const std::string &name, std::string &error) const {
    if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos) {
      error = "'" + name + "' is not a name this sink will store under " + Directory_;
      return false;
    }
    return true;
  }

  std::string Directory_;
};

} // namespace outshine::Render::Parity
#endif
