#include "Check.h"
#include "Json.h"
#include "Script.h"
#include "ReadTextFile.h"
#include <print>
#include <string>
#include <string_view>
#include <cstddef>
#include <utility>

namespace {
constexpr std::string_view kAssertions = R"(
class Test262Error extends Error {}
function $ERROR(message) { throw new Test262Error(message); }
function $DONOTEVALUATE() { throw new Test262Error('unexpected evaluation'); }
function assert(value, message) { if (value !== true) $ERROR(message || 'assertion failed'); }
assert.sameValue = function(a,b,message) { if (!Object.is(a,b)) $ERROR(message || 'different values'); };
assert.notSameValue = function(a,b,message) { if (Object.is(a,b)) $ERROR(message || 'same values'); };
assert.throws = function(type, action, message) {
  try { action(); } catch(error) {
    if (error !== null && error !== undefined && error.constructor === type) return;
    $ERROR(message || 'wrong exception type');
  }
  $ERROR(message || 'missing exception');
};
)";

bool Read(const std::string &path, std::string &text) {
  constexpr size_t kMostCaseBytes = size_t{1} * 1024 * 1024;
  auto input = outshine::ReadTextFile(path, kMostCaseBytes);
  if (!input) { return false; }
  text = std::move(*input);
  return true;
}
}

int main(int argc, const char *const *argv) {
  using namespace outshine;
  using namespace outshine::Test;
  if (argc != 2) {
    CHECK(false, "one prepared corpus case is required");
    return Report();
  }
  const std::string prepared = argv[1];
  std::string manifestText;
  if (!Read(prepared + "/manifest.json", manifestText)) {
    Unprepared(prepared.c_str());
    return Report();
  }
  Json manifest;
  CHECK(manifest.Parse(manifestText.data(), manifestText.size()), "manifest parses");
  if (!manifest.Root().Valid()) { return Report(); }
  const auto root = manifest.Root();
  const auto subject = root["subjects"][size_t{0}];
  bool raw = false;
  bool strict = false;
  for (size_t row = 0; row < subject["attributes"].Size(); ++row) {
    const std::string attribute = subject["attributes"][row].Str();
    raw = raw || attribute == "flags:raw";
    strict = strict || attribute == "flags:onlyStrict";
    if (attribute == "flags:module" || attribute == "flags:async" ||
        attribute == "flags:CanBlockIsTrue" || attribute.starts_with("includes:")) {
      Unprepared(("missing external Test262 harness environment: " + attribute).c_str());
      return Report();
    }
  }
  std::string source;
  if (!Read(prepared + "/" + subject["entry"].Str(), source)) {
    Unprepared(prepared.c_str());
    return Report();
  }
  const std::string prefix = (strict ? "\"use strict\";\n" : std::string()) +
                             (raw ? std::string() : std::string(kAssertions));
  Script::Program program;
  Script::Host host;
  std::string error;
  const bool compiled = program.Read(prefix + source, error);
  const bool negative = root["criterion"]["expects"].StrEquals("refuses");
  const bool parseFailure = negative && root["criterion"]["phase"].StrEquals("parse");
  if (parseFailure) {
    CHECK(!compiled, "negative syntax case is rejected by the actual parser");
  } else {
    CHECK(compiled, error.c_str());
    if (compiled) {
      const bool ran = program.Run(host, error);
      CHECK(negative ? !ran : ran, error.c_str());
    }
  }
  std::println("JS-CASE {} interrupts={}", root["id"].Str(), program.InterruptChecks());
  return Report();
}
