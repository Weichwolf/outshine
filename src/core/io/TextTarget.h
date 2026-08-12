#ifndef TEXTTARGET_H
#define TEXTTARGET_H

#include <cstdio>
#include <memory>
#include <string>

namespace outshine {

/* The two destinations a process always has. A path is the third and there is no fourth. */
enum class TextStream { Stdout, Stderr };

/* WHERE A RUN'S TEXT GOES, and the consumer is the only one that may say. What this replaces was a
 * collector on the far end of a POST: with nothing listening a run wrote 674 lines, delivered none
 * and exited 0, because "the destination is unreachable" was a state the destination itself
 * reported and nobody read.
 *
 * OPEN AT CONSTRUCTION (`C.41`), and a destination that will not open says why: `Refusal()` is
 * empty exactly when `File()` is a stream, so the two cannot disagree and there is nothing to poll.
 * A consumer that ignores it writes to nothing — which is why the one consumer refuses the run. */
class TextTarget {
public:
  explicit TextTarget(TextStream stream) noexcept;
  explicit TextTarget(const std::string &path);

  [[nodiscard]] const std::string &Refusal() const noexcept { return Refusal_; }
  [[nodiscard]] const std::string &Name() const noexcept { return Name_; }
  [[nodiscard]] std::FILE *File() const noexcept { return File_; }

private:
  using Owned = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;

  std::string Name_, Refusal_;
  /* Before File_ (`C.13`): File_ is this handle's stream when the target is a path, and the process
   * streams are never owned. */
  Owned Owned_{nullptr, &std::fclose};
  std::FILE *File_ = nullptr;
};

} // namespace outshine
#endif
