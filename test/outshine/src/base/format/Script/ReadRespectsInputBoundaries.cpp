#include "Script.h"
#include "Check.h"
#include <array>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string_view, 9> valid{
      "", " ", "42", "clicked()", "clicked(); \t\n", "// eof", "/* done */", "'text'", "a=1; b=2"};
  Script::Program program;
  std::string error;
  for (const std::string_view source : valid) {
    error.clear();
    CHECK(program.Read(source, error) && error.empty(), "complete views stop without a sentinel");
  }
  const std::array<char, 10> bytes{'c', 'l', 'i', 'c', 'k', 'e', 'd', '(', ')', '@'};
  error.clear();
  CHECK(program.Read(std::string_view(bytes.data(), 9), error) && error.empty(),
        "the byte beyond a nonterminated view is never consumed");
  CHECK(!program.Read(std::string_view(bytes.data(), bytes.size()), error) && !error.empty(),
        "the same byte is rejected when it belongs to the input");
  error.clear();
  CHECK(!program.Read("'unterminated", error) && !error.empty(),
        "truncated strings still report a parse failure");
  error.clear();
  CHECK(program.Read("clicked()", error), "the parser recovers after a rejected input");
  return Report();
}
