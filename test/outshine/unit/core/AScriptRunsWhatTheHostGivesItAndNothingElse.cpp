/* THE INTERPRETER, AND THE LINE IT DOES NOT CROSS (board:1448).
 *
 * **THE LIBRARY EXECUTES AND THE CONSUMER SUPPLIES THE WORLD.** Every claim below hands the same
 * interpreter a different host and gets a different language -- which is the whole design stated as a
 * test: there is no `document` in `src/core/Script.cpp`, and there is one here because this file put
 * it there.
 *
 * **THE SUBSET IS TESTED IN BOTH DIRECTIONS.** What is in, by running it; what is out, by the refusal
 * naming it. A capability list nobody exercises the far side of is a list of hopes. */
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Script.h"

using namespace outshine::Test;
using namespace outshine::Script;

namespace {

/* A HOST THAT IS A DOCUMENT, and this file is the only place in the tree that word appears beside this
 * interpreter. `document.getElementById("x").style.height = "100px"` is four host answers and one
 * assignment, and not one line of it is knowledge the library carries. */
class Document final : public Host {
public:
  struct Element {
    std::string Id;
    std::string Height;
    std::string Width;
  };
  std::vector<Element> Elements;
  int Calls = 0;

  [[nodiscard]] Value Global(const std::string &name) override {
    if (name == "document") { return Value::OfRef(kDocument); }
    return {};
  }
  [[nodiscard]] Value Member(const Value &object, const std::string &name) override {
    if (object.What != Kind::Ref) { return {}; }
    if (object.Ref == kDocument && name == "getElementById") { return Value::OfRef(kLookup); }
    if (object.Ref >= kFirstElement && name == "style") {
      /* A STYLE IS A HANDLE ONTO THE SAME ELEMENT, one range higher. The host decides that; the
       * interpreter carries an integer it never looks inside. */
      return Value::OfRef(object.Ref + kStyleShift);
    }
    if (object.Ref >= kFirstElement && name == "offsetHeight") {
      return Value::OfNumber((double)(object.Ref - kFirstElement) * 10.0);
    }
    return {};
  }
  [[nodiscard]] bool SetMember(const Value &object, const std::string &name,
                               const Value &to) override {
    if (object.What != Kind::Ref || object.Ref < kFirstElement + kStyleShift) { return false; }
    const size_t at = (size_t)(object.Ref - kFirstElement - kStyleShift);
    if (at >= Elements.size()) { return false; }
    if (name == "height") {
      Elements[at].Height = to.AsText();
      return true;
    }
    if (name == "width") {
      Elements[at].Width = to.AsText();
      return true;
    }
    return false;
  }
  [[nodiscard]] bool Call(const Value &callee, const Value *args, size_t count,
                          Value &out) override {
    if (callee.What != Kind::Ref || callee.Ref != kLookup || count != 1) { return false; }
    ++Calls;
    for (size_t at = 0; at < Elements.size(); ++at) {
      if (Elements[at].Id == args[0].AsText()) {
        out = Value::OfRef(kFirstElement + (int)at);
        return true;
      }
    }
    out = Value();
    return true;
  }

private:
  static constexpr int kDocument = 1;
  static constexpr int kLookup = 2;
  static constexpr int kFirstElement = 100;
  static constexpr int kStyleShift = 1000;
};

/* THE SMALLEST USEFUL HOST: one name and one call. It exists to show that a consumer implementing one
 * method has a language with one word in it, which is what the defaults in `Host` are for. */
class Adder final : public Host {
public:
  [[nodiscard]] Value Global(const std::string &name) override {
    return name == "twice" ? Value::OfRef(1) : Value();
  }
  [[nodiscard]] bool Call(const Value &callee, const Value *args, size_t count,
                          Value &out) override {
    if (callee.What != Kind::Ref || count != 1) { return false; }
    out = Value::OfNumber(args[0].Number * 2.0);
    return true;
  }
};

/* A host that answers nothing at all, which is the default `Host` and the strictest one. */
class Empty final : public Host {};

bool Runs(const char *text, Host &host, std::string &error) {
  Program program;
  if (!program.Read(text, error)) { return false; }
  return program.Run(host, error);
}

}  // namespace

int main(void) {
  /* ARITHMETIC, COMPARISON AND TRUTH, and every answer is read back through a name the script
   * assigned -- which is also the claim that a name assigned is a name that exists. */
  {
    Program program;
    std::string error;
    CHECK(program.Read("a = 2 + 3 * 4;\n"
                       "b = (2 + 3) * 4;\n"
                       "c = 7 % 4;\n"
                       "d = 1 < 2 && 3 >= 3;\n"
                       "e = !0;\n"
                       "f = 10 / 4;\n"
                       "g = 1 == 1.0;\n",
                       error),
          "the script reads");
    Empty nothing;
    CHECK(program.Run(nothing, error), "and runs against a host that answers nothing");
    const auto number = [&program](const char *name) {
      const Value *held = program.Named(name);
      return held != nullptr ? held->Number : -1.0;
    };
    CHECK(number("a") == 14, "multiplication binds tighter than addition");
    CHECK(number("b") == 20, "and parentheses say otherwise");
    CHECK(number("c") == 3, "the remainder is the remainder");
    CHECK(number("d") == 1, "a conjunction of two true comparisons is true");
    CHECK(number("e") == 1, "and zero is false, so its negation is true");
    CHECK(std::fabs(number("f") - 2.5) < 1e-12, "division is not integer division");
    CHECK(number("g") == 1, "a number equals itself whatever it is written as");
  }

  /* `+` IS TWO OPERATIONS AND TEXT WINS, which is what makes `100 + "px"` the string a length is
   * written as -- and an integral number is written without a point, or a host would be handed
   * `100.000000px`. */
  {
    Program program;
    std::string error;
    CHECK(program.Read("a = 100 + \"px\"; b = \"n=\" + (2 + 3); c = 2.5 + \"x\";", error),
          "the script reads");
    Empty nothing;
    CHECK(program.Run(nothing, error), "and runs");
    const auto text = [&program](const char *name) {
      const Value *held = program.Named(name);
      return held != nullptr ? held->Text : std::string("<absent>");
    };
    CHECK(text("a") == "100px", "an integral number joins text without a decimal point");
    CHECK(text("b") == "n=5", "and the arithmetic happens before the joining");
    CHECK(text("c") == "2.5x", "a number that is not integral keeps what it needs");
  }

  /* CONTROL FLOW, AND `while` IS BOUNDED BY THE STEP COUNT AND NOTHING ELSE. A loop that cannot end is
   * refused rather than run, which is what a declared bound BUYS. */
  {
    Program program;
    std::string error;
    CHECK(program.Read("n = 0; i = 0;\n"
                       "while (i < 5) { n = n + i; i = i + 1; }\n"
                       "if (n == 10) { answer = \"ten\"; } else { answer = \"other\"; }\n",
                       error),
          "the script reads");
    Empty nothing;
    CHECK(program.Run(nothing, error), "and runs");
    const Value *answer = program.Named("answer");
    CHECK(answer != nullptr && answer->Text == "ten", "the loop summed and the branch chose");
    std::printf("NOTE the loop took %zu steps of the %zu allowed\n", program.Steps(), kMaxSteps);
  }
  {
    Program program;
    std::string error;
    CHECK(program.Read("while (1) { n = 1; }", error), "a script may declare a loop with no end");
    Empty nothing;
    CHECK(!program.Run(nothing, error), "and running it is refused rather than never returning");
    CHECK(error.find("step bound") != std::string::npos,
          "and the refusal names the bound it reached");
  }

  /* THE HOST IS THE WORLD, and the same interpreter is a different language for each one. */
  {
    Document document;
    document.Elements.push_back({"outer", "", ""});
    document.Elements.push_back({"inner", "", ""});
    std::string error;
    CHECK(Runs("var outer = document.getElementById(\"outer\");\n"
               "outer.style.height = \"100px\";\n"
               "outer.style.width = 40 + 60 + \"px\";\n"
               "document.getElementById(\"inner\").style.height = \"7px\";\n",
               document, error),
          "a document's own script runs against a host that is a document");
    CHECK(document.Elements[0].Height == "100px", "an element's style is set through the host");
    CHECK(document.Elements[0].Width == "100px", "and an expression reaches it as the text it makes");
    CHECK(document.Elements[1].Height == "7px", "a call chains into a member and an assignment");
    CHECK(document.Calls == 2, "and the host saw exactly the calls the script made");
  }
  {
    Adder adder;
    std::string error;
    Program program;
    CHECK(program.Read("a = twice(21);", error), "the same interpreter reads a different language");
    CHECK(program.Run(adder, error), "and runs it against a host with one word in it");
    const Value *a = program.Named("a");
    CHECK(a != nullptr && a->Number == 42, "the host's own call answered");
  }

  /* A CALL THE HOST DOES NOT KNOW IS A REFUSAL. A call that quietly did nothing is a script that
   * appeared to work and changed nothing, which is the worst of the three outcomes. */
  {
    Empty nothing;
    std::string error;
    CHECK(!Runs("missing(1);", nothing, error), "a call no host answers is refused");
    CHECK(error.find("does not answer this call") != std::string::npos,
          "and the refusal says so rather than reporting success");
    CHECK(!Runs("a = 1; a.b = 2;", nothing, error), "a write no host takes is refused too");
  }

  /* `&&` AND `||` DO NOT REACH THEIR RIGHT SIDE UNLESS THEY MUST, which is not an optimisation: it is
   * how a script guards a member read, and evaluating both would make the guard the failure. */
  {
    Empty nothing;
    std::string error;
    Program program;
    CHECK(program.Read("a = 0 && missing(1); b = 1 || missing(1);", error), "the guards read");
    CHECK(program.Run(nothing, error),
          "and run, because neither right side was reached -- a call no host answers sits behind "
          "both, and reaching either would have refused");
  }

  /* WHAT IS OUTSIDE THE SUBSET, BY THE REFUSAL NAMING IT. Each of these is a decision written down in
   * board:1448, and a subset whose far side nobody exercises is a list of hopes. */
  {
    Empty nothing;
    std::string error;
    struct Outside {
      const char *Text;
      const char *What;
    };
    const Outside kOutside[] = {
        {"function f() { return 1; }", "a function the script defines"},
        {"a = [1, 2];", "an array the script makes"},
        {"for (i = 0; i < 3; i = i + 1) { }", "a for statement"},
        {"a = 1 ? 2 : 3;", "a conditional expression"},
        {"a = `x`;", "a template literal"},
        {"try { a = 1; } catch (e) { }", "a try statement"},
    };
    int refused = 0;
    for (const Outside &one : kOutside) {
      Program program;
      const bool read = program.Read(one.Text, error);
      const bool ran = read && program.Run(nothing, error);
      refused += ran ? 0 : 1;
      Checked(!ran, "outside the subset is refused",
              (std::string(one.What) + " is outside the subset and the interpreter says so").c_str(),
              __FILE__, __LINE__);
    }
    std::printf("NOTE constructs outside the subset refused = %d of %zu\n", refused,
                sizeof(kOutside) / sizeof(kOutside[0]));
  }

  /* THE PARSE BOUNDS ARE REACHED AND NAMED, not silently survived. */
  {
    std::string deep;
    for (size_t at = 0; at < kMaxDepth + 8; ++at) { deep += "("; }
    deep += "1";
    for (size_t at = 0; at < kMaxDepth + 8; ++at) { deep += ")"; }
    Program program;
    std::string error;
    CHECK(!program.Read(deep + ";", error), "a script nested past the depth bound is refused");
    CHECK(error.find("depth bound") != std::string::npos, "and the refusal names the bound");
  }

  /* A SCRIPT THAT WAS NEVER READ IS NOT A SCRIPT THAT DID NOTHING. */
  {
    Program program;
    Empty nothing;
    std::string error;
    CHECK(!program.Run(nothing, error), "running an unread program is refused");
  }

  /* AN ACTOR REMEMBERS BETWEEN TICKS, because its program IS its memory. A railway that drives by
   * itself and stops at a station now and then is a handful of names carried from one run to the next,
   * and the consumer holds them -- bounded, readable, and clearable when it wants a clean slate. */
  {
    Program tick;
    std::string error;
    CHECK(tick.Read("phase = phase + 1; if (phase > 3) { phase = 0; stops = stops + 1; }", error),
          "a tick reads");
    Empty nothing;
    for (int at = 0; at < 9; ++at) { CHECK(tick.Run(nothing, error), "and runs again"); }
    const Value *phase = tick.Named("phase");
    const Value *stops = tick.Named("stops");
    CHECK(phase != nullptr && stops != nullptr, "the names it assigned are still there");
    if (phase != nullptr && stops != nullptr) {
      CHECK(stops->Number == 2,
            "nine ticks of a four-phase cycle stopped twice -- which is only countable because the "
            "state survived the run");
      CHECK(phase->Number == 1, "and the phase is where the ninth tick left it");
    }
    tick.Reset();
    CHECK(tick.Named("phase") == nullptr, "and Reset is the explicit door to a clean slate");
  }

  return Report();
}
