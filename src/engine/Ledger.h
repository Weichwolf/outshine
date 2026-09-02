#ifndef OUTSHINE_ENGINE_LEDGER_H
#define OUTSHINE_ENGINE_LEDGER_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <scenario/Event.h>

namespace outshine::Core {

class Ledger {
public:
  void Stands(std::vector<Measure> declared) {
    Numbers_ = std::move(declared);
    Standing_ = Numbers_.size();
    Rounds_.assign(Numbers_.size(), 0);
    Round_ = 0;
    Clashed_.clear();
  }

  void Opens() { ++Round_; }

  [[nodiscard]] const std::vector<std::string> &Clashed() const { return Clashed_; }

  void Places(const std::string &what, double how, const char *unit) {
    Places(what.c_str(), how, unit);
  }

  void Places(const char *what, double how, const char *unit) {
    for (size_t at = Standing_; at < Numbers_.size(); ++at) {
      if (Numbers_[at].What == what) {
        if (Rounds_[at] == Round_) {
          if (Numbers_[at].How != how && std::ranges::find(Clashed_, what) == Clashed_.end()) {
            Clashed_.emplace_back(what);
          }
          return;
        }
        Rounds_[at] = Round_;
        Numbers_[at].How = how;
        return;
      }
    }
    Numbers_.push_back(Measure{.What = what, .How = how, .Unit = unit});
    Rounds_.push_back(Round_);
  }

  [[nodiscard]] const std::vector<Measure> &Numbers() const { return Numbers_; }

private:
  std::vector<Measure> Numbers_;
  std::vector<uint64_t> Rounds_;
  uint64_t Round_ = 0;
  std::vector<std::string> Clashed_;
  size_t Standing_ = 0;
};

} // namespace outshine::Core
#endif
