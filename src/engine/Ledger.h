#ifndef OUTSHINE_ENGINE_LEDGER_H
#define OUTSHINE_ENGINE_LEDGER_H

#include <string>
#include <utility>
#include <vector>

#include <Event.h>

namespace outshine::Core {

class Ledger {
public:
  void Stands(std::vector<Measure> declared) {
    Numbers_ = std::move(declared);
    Standing_ = Numbers_.size();
  }

  void Places(const std::string &what, double how, const char *unit) {
    Places(what.c_str(), how, unit);
  }

  void Places(const char *what, double how, const char *unit) {
    for (size_t at = Standing_; at < Numbers_.size(); ++at) {
      if (Numbers_[at].What == what) {
        Numbers_[at].How = how;
        return;
      }
    }
    Numbers_.push_back(Measure{what, how, unit});
  }

  [[nodiscard]] const std::vector<Measure> &Numbers() const { return Numbers_; }

private:
  std::vector<Measure> Numbers_;
  size_t Standing_ = 0;
};

}
#endif
