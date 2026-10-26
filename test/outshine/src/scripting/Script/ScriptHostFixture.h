#ifndef OUTSHINE_TEST_SCRIPT_HOSTFIXTURE_H
#define OUTSHINE_TEST_SCRIPT_HOSTFIXTURE_H

#include "Script.h"
#include <span>
#include <string_view>
#include <vector>

struct ScriptHostFixture : outshine::Script::Host {
  using Value = outshine::Script::Value;
  std::vector<std::vector<Value>> Calls;
  Value Property = Value::OfNumber(40);
  bool Accept = true;

  Value Global(std::string_view name) override {
    if (name == "capture") { return Value::OfRef(1); }
    if (name == "object") { return Value::OfRef(2); }
    return {};
  }

  Value Member(const Value &holder, std::string_view name) override {
    if (holder.Ref != 2) { return {}; }
    if (name == "value") { return Property; }
    return name == "record" ? Value::OfRef(1) : Value{};
  }

  bool SetMember(const Value &holder, std::string_view name, const Value &value) override {
    if (holder.Ref != 2 || name != "value") { return false; }
    Property = value;
    return true;
  }

  bool Call(const Value &callee, std::span<const Value> args, Value &out) override {
    if (!Accept || callee.Ref != 1) { return false; }
    Calls.emplace_back(args.begin(), args.end());
    out = args.empty() ? Value{} : args.front();
    return true;
  }
};

#endif
