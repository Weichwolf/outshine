/* THE HARNESS FOR tc39/test262, fetched at the pin every manifest cites (board:1450).
 *
 * **A CASE IS DECIDED BY WHAT IT DECLARES ABOUT ITSELF.** test262 opens every file with a comment
 * block carrying its own frontmatter: a case with no `negative` entry passes by RUNNING TO THE END, and one with a negative block passes
 * by being REFUSED with the kind of refusal it named. Nothing here decides what a case means.
 *
 * **THE HARNESS'S CONTRACT IS PROVIDED AS NATIVES AND NOT AS SCRIPT TEXT.** `assert.js` and `sta.js`
 * define functions, which the subset writes down as outside -- so running them would fail every case
 * at the parser for a reason that has nothing to do with the case. The host below implements what they
 * promise, and the case's own text is run unmodified. **A name the host does not provide is OUTSIDE
 * THE SUBSET and never a failure**, which is the difference between *we cannot decide this* and *this
 * is wrong*.
 *
 * **A NEGATIVE `parse` CASE IS OUTSIDE THE SUBSET AND THAT IS A DECISION.** This parser refuses a
 * VALID program that reaches past the subset with the same voice it refuses an invalid one, so it
 * cannot tell the two apart -- and a case that passed by refusing for the wrong reason would be a
 * green light about something else. It is the `(case, metric)` ladder's second rung, taken openly. */
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "Json.h"
#include "Script.h"

namespace {

using outshine::Json;
namespace S = outshine::Script;

/* WHAT THE HOST ANSWERS, and the whole of it. Every name here is one `assert.js` or `sta.js` promises;
 * a case reaching anything else is outside the subset and says which name. */
class Test262Host final : public S::Host {
public:
  std::string Unprovided;   /* the first name this host could not answer */
  std::string Failed;       /* the first assertion that did not hold */

  [[nodiscard]] S::Value Global(const std::string &name) override {
    if (name == "assert") { return S::Value::OfRef(kAssert); }
    if (name == "$ERROR" || name == "$DONOTEVALUATE") { return S::Value::OfRef(kError); }
    if (name == "print") { return S::Value::OfRef(kPrint); }
    if (name == "undefined") { return {}; }
    if (Unprovided.empty()) { Unprovided = "name:" + name; }
    return {};
  }
  [[nodiscard]] S::Value Member(const S::Value &object, const std::string &name) override {
    if (object.What == S::Kind::Ref && object.Ref == kAssert) {
      if (name == "sameValue") { return S::Value::OfRef(kSame); }
      if (name == "notSameValue") { return S::Value::OfRef(kNotSame); }
      if (Unprovided.empty()) { Unprovided = "name:assert." + name; }
      return {};
    }
    if (Unprovided.empty()) { Unprovided = "name:" + name; }
    return {};
  }
  [[nodiscard]] bool Call(const S::Value &callee, const S::Value *args, size_t count,
                          S::Value &out) override {
    if (callee.What != S::Kind::Ref) { return false; }
    out = S::Value();
    switch (callee.Ref) {
      case kAssert:
        if (count >= 1 && !args[0].Truth()) {
          Failed = count >= 2 ? args[1].AsText() : "an assertion did not hold";
          return false;
        }
        return true;
      case kSame:
      case kNotSame: {
        if (count < 2) { return false; }
        const bool same = args[0].What == S::Kind::Text || args[1].What == S::Kind::Text
                              ? args[0].AsText() == args[1].AsText()
                              : args[0].Number == args[1].Number;
        if (same != (callee.Ref == kSame)) {
          Failed = (count >= 3 ? args[2].AsText() + ": " : std::string()) + "got " +
                   args[0].AsText() + ", wanted " +
                   (callee.Ref == kSame ? "" : "anything but ") + args[1].AsText();
          return false;
        }
        return true;
      }
      case kError:
        Failed = count >= 1 ? args[0].AsText() : "the case called $ERROR";
        return false;
      case kPrint:
        return true;
      default: break;
    }
    return false;
  }

private:
  static constexpr int kAssert = 1;
  static constexpr int kSame = 2;
  static constexpr int kNotSame = 3;
  static constexpr int kError = 4;
  static constexpr int kPrint = 5;
};

std::string ReadFile(const std::string &path, bool &found) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    found = false;
    return {};
  }
  std::string text;
  char buffer[1 << 15];
  size_t got = 0;
  while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) { text.append(buffer, got); }
  std::fclose(f);
  found = true;
  return text;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    outshine::Test::Checked(false, "argc == 2", "one prepared case directory is the argument",
                            __FILE__, __LINE__);
    return outshine::Test::Report();
  }
  const std::string prepared = argv[1];

  bool found = false;
  const std::string manifestText = ReadFile(prepared + "/manifest.json", found);
  if (!found) {
    outshine::Test::Unprepared(prepared.c_str());
    return outshine::Test::Report();
  }
  Json manifest;
  if (!manifest.Parse(manifestText.c_str(), manifestText.size())) {
    outshine::Test::Checked(false, "the manifest parses", prepared.c_str(), __FILE__, __LINE__);
    return outshine::Test::Report();
  }
  const Json::Ref root = manifest.Root();
  const std::string id = root["id"].Str();
  const Json::Ref subject = root["subjects"][size_t(0)];
  const std::string entry = subject["entry"].Str();
  const bool wantsRefusal = root["criterion"]["expects"].StrEquals("refuses");
  const std::string phase = root["criterion"]["phase"].Str();

  /* WHAT THE CASE SAYS IT NEEDS, before a line of it is read. A flag or an include this runner does
   * not provide puts the case outside the subset by the case's OWN declaration, which is the cheapest
   * and most honest place to decide it. */
  std::vector<std::string> wanted;
  for (size_t at = 0; at < subject["attributes"].Size(); ++at) {
    const std::string attribute = subject["attributes"][at].Str();
    /* A FLAG ABOUT WHICH MODE TO RUN IN IS NOTHING TO THIS INTERPRETER, which has one mode. The rest
     * of what a case declares is a requirement, and a requirement it cannot meet is a name. */
    const bool nothingToUs = attribute == "flags:raw" || attribute == "flags:generated" ||
                             attribute == "flags:CanBlockIsFalse" ||
                             attribute == "flags:CanBlockIsTrue";
    if (!nothingToUs) { wanted.push_back(attribute); }
  }
  if (wantsRefusal && phase != "runtime") {
    wanted.push_back("negative-" + (phase.empty() ? std::string("parse") : phase));
  }

  const std::string script = ReadFile(prepared + "/" + entry, found);
  if (!found) {
    outshine::Test::Unprepared((prepared + " -- " + entry).c_str());
    return outshine::Test::Report();
  }

  /* **A BOUNDARY AND A GAP ARE TWO ANSWERS, AND THE LANGUAGE'S OWN TABLE SEPARATES THEM.** A case
   * this interpreter declines because a game's handler will never define a class is FINISHED; one it
   * declines because something is missing is WAITING. An undeclared name is RED, which is what stops
   * this from being a rubber stamp: the only way to make a case green is to build the capability or to
   * write the boundary down with its reason. */
  const auto settle = [&id](const std::vector<std::string> &names) {
    std::string boundary, gap;
    for (const std::string &name : names) {
      const char *why = S::WhyOutside(name);
      std::string &into = why != nullptr ? boundary : gap;
      if (!into.empty()) { into += " "; }
      into += name;
      if (why != nullptr) { into += " (" + std::string(why) + ")"; }
    }
    if (gap.empty()) {
      std::printf("JS-SUBSET reduced\n");
      std::printf("REDUCED %s -- every name that puts it outside is a declared boundary: %s\n",
                  id.c_str(), boundary.c_str());
      outshine::Test::Checked(true, "the case is outside a boundary this language declared",
                              (id + ": " + boundary).c_str(), __FILE__, __LINE__);
    } else {
      std::printf("JS-SUBSET outside\n");
      std::printf("OUTSIDE %s -- undeclared: %s\n", id.c_str(), gap.c_str());
      outshine::Test::Checked(false, "every name that puts a case outside is declared",
                              (id + ": " + gap + " is outside the subset and nothing says why")
                                  .c_str(),
                              __FILE__, __LINE__);
    }
    return outshine::Test::Report();
  };

  if (!wanted.empty()) { return settle(wanted); }

  S::Program program;
  std::string error;
  if (!program.Read(script, error)) {
    /* A PARSE REFUSAL NAMES THE TOKEN IT STOPPED ON, and the token is what the boundary table reads.
     * The message says where and why in a sentence; a sentence cannot be looked up. */
    return settle({program.Stopped().empty() ? error : "token:" + program.Stopped()});
  }

  Test262Host host;
  const bool ran = program.Run(host, error);
  if (!host.Unprovided.empty()) { return settle({host.Unprovided}); }

  std::printf("JS-SUBSET inside\n");
  char why[512];
  if (wantsRefusal) {
    std::snprintf(why, sizeof why, "%s: upstream states this case must fail at runtime, and it %s",
                  id.c_str(), ran ? "ran to its end" : "was refused");
    outshine::Test::Checked(!ran, "a negative case is refused", why, __FILE__, __LINE__);
  } else {
    std::snprintf(why, sizeof why, "%s: %s", id.c_str(),
                  ran ? "ran to its end" : (host.Failed.empty() ? error : host.Failed).c_str());
    outshine::Test::Checked(ran, "a positive case runs to its end", why, __FILE__, __LINE__);
  }
  std::printf("JS-CASE %s\n", outshine::Test::Failures.Value() == 0 ? "held" : "red");
  std::printf("STEPS %s %zu\n", id.c_str(), program.Steps());
  return outshine::Test::Report();
}
