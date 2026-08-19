/* A SMALL BOUNDED INTERPRETER, AND IT KNOWS NOTHING ABOUT WHAT IT IS DRIVING (board:1448).
 *
 * **THE LIBRARY EXECUTES AND THE CONSUMER SUPPLIES THE WORLD.** Every name a script can reach comes
 * from a `Host` the consumer implements: what `document` is, what a member means, what a call does.
 * Nothing here knows a document, an element, an actor or a button -- which is the same rule the
 * renderer follows about content nouns, applied to a language.
 *
 * **EVERYTHING THAT GROWS STATES ITS BOUND, and each is a number somebody chose.** A parse is bounded
 * in tokens, nodes and nesting; a run is bounded in steps. A script that reaches one is REFUSED with
 * the bound named, never truncated -- a program cut in half is a program that did something else.
 *
 * **A PARSE IS ONCE AND A RUN IS MANY.** `Program::Read` produces a tree; `Run` walks it. That is what
 * keeps a handler off the parser on every event, and it is why the text may go once it is read.
 *
 * **THE SUBSET IS WRITTEN DOWN IN BOTH DIRECTIONS.** In: numbers, quoted text, variables, member
 * reads and writes, calls, `if`/`else`, `while`, the arithmetic and comparison operators, `&&`, `||`
 * and `!`. Out and named: functions a script defines, objects a script makes, arrays, `for`, closures,
 * exceptions, `var`/`let` scoping rules -- a script here assigns a name and the name exists. Each of
 * those is a decision and not an omission: this is a language for a handler and a fixture, and every
 * one of them is how a handler becomes a program nobody can bound. */
#ifndef SCRIPT_H
#define SCRIPT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Script {

/* [SET] THE BOUNDS. A handler is a few lines and a fixture is a few dozen; these are far above both
 * and far below anything that could run for a visible time. */
inline constexpr size_t kMaxTokens = 4096;
inline constexpr size_t kMaxNodes = 2048;
inline constexpr size_t kMaxDepth = 32;
inline constexpr size_t kMaxSteps = 100000;
inline constexpr size_t kMaxNames = 128;
inline constexpr size_t kMaxArgs = 8;

enum class Kind : uint8_t {
  Nothing,
  Number,
  Text,
  /* AN OPAQUE HANDLE THE HOST OWNS. The interpreter passes it around and never looks inside, which is
   * what lets a consumer put a document, an element or an actor behind one without this file learning
   * any of those words. */
  Ref,
};

struct Value {
  Kind What = Kind::Nothing;
  double Number = 0.0;
  std::string Text;
  int Ref = 0;

  static Value OfNumber(double number) {
    Value out;
    out.What = Kind::Number;
    out.Number = number;
    return out;
  }
  static Value OfText(std::string text) {
    Value out;
    out.What = Kind::Text;
    out.Text = std::move(text);
    return out;
  }
  static Value OfRef(int ref) {
    Value out;
    out.What = Kind::Ref;
    out.Ref = ref;
    return out;
  }
  /* WHAT A CONDITION MAKES OF IT, and it is stated rather than inherited from a language nobody named:
   * zero and empty text are false, a handle is true, nothing is false. */
  [[nodiscard]] bool Truth(void) const;
  /* THE TEXT A NUMBER IS WRITTEN AS when a host wants one -- integral values without a point, so
   * `100` is `"100"` and not `"100.000000"`. */
  [[nodiscard]] std::string AsText(void) const;
};

/* WHAT THE SCRIPT CAN REACH, AND THE CONSUMER DECIDES ALL OF IT. Every default answers *nothing*, so a
 * host that implements one method has a language with one name in it and nothing else -- which is the
 * smallest useful host and the one a test writes. */
struct Host {
  virtual ~Host() = default;
  /* A free name the script did not assign: `document`, `world`, `print`. */
  [[nodiscard]] virtual Value Global(const std::string &name) {
    (void)name;
    return {};
  }
  [[nodiscard]] virtual Value Member(const Value &object, const std::string &name) {
    (void)object;
    (void)name;
    return {};
  }
  [[nodiscard]] virtual bool SetMember(const Value &object, const std::string &name,
                                       const Value &to) {
    (void)object;
    (void)name;
    (void)to;
    return false;
  }
  /* `callee` is whatever `Global` or `Member` produced. **A host that does not know it answers false**,
   * and the run refuses naming the call -- a call that quietly did nothing is the defect this returns
   * a bool to prevent. */
  [[nodiscard]] virtual bool Call(const Value &callee, const Value *args, size_t count, Value &out) {
    (void)callee;
    (void)args;
    (void)count;
    (void)out;
    return false;
  }
};

/* WHY A CONSTRUCT OR A NAME IS DELIBERATELY OUTSIDE, or `nullptr` where nothing accounts for it
 * (board:1448).
 *
 * **THIS IS THE DIFFERENCE BETWEEN A BOUNDARY AND A GAP**, the same one the UI subset draws: a script
 * this interpreter declines because a game's handler will never define a class is FINISHED; one it
 * declines because nobody built `while` would be WAITING. Both read as *outside the subset* to a
 * counter, and only this table separates them.
 *
 * The name is `token:<what the parser stopped on>`, `name:<a global the host did not answer>`, or one
 * of the words a corpus case declares about itself. */
[[nodiscard]] const char *WhyOutside(const std::string &name);

class Program {
public:
  /* THE TREE'S NODE. Its NAME is public and its DEFINITION is in the implementation: the parser is a
   * free function there, and a friend declaration in a header cannot reach a type in an anonymous
   * namespace. Nothing outside this library can build one or read one, which is the encapsulation
   * that was wanted -- the name alone carries nothing. */
  struct Node;

  /* THE SPECIAL MEMBERS ARE DECLARED HERE AND DEFINED WHERE `Node` IS COMPLETE. A vector of an
   * incomplete type is only conditionally supported, and the condition is that nothing instantiates
   * its destructor -- which every translation unit including this header would otherwise do. */
  Program();
  ~Program();
  Program(const Program &) = delete;
  Program &operator=(const Program &) = delete;
  Program(Program &&) noexcept;
  Program &operator=(Program &&) noexcept;

  /* Reads `text` into a tree. A refusal names what was expected, what was found, and where. */
  [[nodiscard]] bool Read(const std::string &text, std::string &error);
  [[nodiscard]] bool Held(void) const;
  /* HOW MANY NODES THE PARSE PRODUCED, published because a bound that nobody can see the distance to
   * is a bound nobody can act on. */
  [[nodiscard]] size_t NodeCount(void) const;

  /* Runs the tree against `host`. A refusal names the step that could not be taken. */
  [[nodiscard]] bool Run(Host &host, std::string &error);
  /* HOW MANY STEPS THE LAST RUN TOOK, against `kMaxSteps`. */
  [[nodiscard]] size_t Steps(void) const { return Steps_; }
  /* A name the script assigned, for a consumer that wants an answer out of one. */
  [[nodiscard]] const Value *Named(const std::string &name) const;
  /* THE TOKEN A REFUSED PARSE STOPPED ON, as the script spelled it. The message says where and why in
   * a sentence; this says WHAT in one word, which is what a caller needs to decide whether the
   * construct is one this engine declines on purpose or one nobody has built. A sentence cannot be
   * looked up in a table and a token can. */
  [[nodiscard]] const std::string &Stopped(void) const { return Stopped_; }

private:
  [[nodiscard]] bool Evaluate(size_t node, Host &host, Value &out, std::string &error);
  [[nodiscard]] bool Perform(size_t node, Host &host, std::string &error);

  std::vector<Node> Nodes_;
  size_t Root_ = 0;
  std::vector<std::string> Names_;
  std::vector<Value> Held_;
  size_t Steps_ = 0;
  std::string Stopped_;
};

} // namespace outshine::Script
#endif
