#ifndef OUTSHINE_ENGINE_ACTIONHOSTADAPTER_H
#define OUTSHINE_ENGINE_ACTIONHOSTADAPTER_H

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <scenario/Event.h>
#include "Script.h"

namespace outshine {

class ActionHostAdapter final : public Script::Host {
public:
  explicit ActionHostAdapter(outshine::Host *client) : Client_(client) {}

  [[nodiscard]] Script::Value Global(std::string_view name) override {
    for (size_t at = 0; at < Named_.size(); ++at) {
      if (Named_[at] == name) { return Script::Value::OfRef(static_cast<int>(at) + 1); }
    }
    if (Named_.size() >= Script::kMaxNames) { return {}; }
    Named_.emplace_back(name);
    return Script::Value::OfRef(static_cast<int>(Named_.size()));
  }

  [[nodiscard]] bool Call(const Script::Value &callee,
                          std::span<const Script::Value> args,
                          Script::Value &out) override {
    out = Script::Value();
    if (Client_ == nullptr || callee.What != Script::Kind::Ref || args.size() > Script::kMaxArgs) {
      return false;
    }
    const size_t which = static_cast<size_t>(callee.Ref) - 1;
    if (callee.Ref <= 0 || which >= Named_.size()) { return false; }

    std::array<Argument, Script::kMaxArgs> handed{};
    for (size_t at = 0; at < args.size(); ++at) {
      if (args[at].What == Script::Kind::Text) {
        handed[at].Is = Argument::Kind::Text;
        handed[at].Text = args[at].Text;
      } else if (args[at].What == Script::Kind::Number) {
        handed[at].Is = Argument::Kind::Number;
        handed[at].Number = args[at].Number;
      } else {
        return false;
      }
    }
    Fired_ = true;
    return Client_->calls(Named_[which], std::span<const Argument>(handed.data(), args.size()));
  }

  [[nodiscard]] bool Fired() const { return Fired_; }

private:
  outshine::Host *Client_ = nullptr;
  std::vector<std::string> Named_;
  bool Fired_ = false;
};

}

#endif
