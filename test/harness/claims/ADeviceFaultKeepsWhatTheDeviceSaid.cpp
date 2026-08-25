#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const char *path) {
  std::string out;
  std::FILE *const file = std::fopen(path, "rb");
  if (file == nullptr) { return out; }
  char block[8192];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { out.append(block, read); }
  std::fclose(file);
  return out;
}

[[nodiscard]] size_t LineOf(const std::string &text, size_t at) {
  size_t line = 1;
  for (size_t scan = 0; scan < at && scan < text.size(); ++scan) {
    line += text[scan] == '\n' ? 1 : 0;
  }
  return line;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1847 moved every refusal on the render door to a string_view into static text, so
  // nothing allocates where a caller reads it. board:1853 is the other half: SDL_GetError() is
  // THREAD-LOCAL and the next SDL call overwrites it, so a fault the DEVICE reported and did
  // not copy has lost the one thing the caller could not have worked out from its own
  // arguments. A fault the caller caused -- a null window, an extent of nothing -- says itself.
  const std::string source = Slurp("src/render/Renderer.cpp");
  CHECK(source.size() > 1000, "the renderer's body was read, so this walk judges the code");

  std::vector<std::string> silent;
  size_t asked = 0, kept = 0;
  for (size_t at = source.find("SDL_GetError()"); at != std::string::npos;
       at = source.find("SDL_GetError()", at + 1)) {
    const size_t opens = source.rfind('\n', at);
    const size_t begins = opens == std::string::npos ? 0 : opens + 1;
    const std::string line = source.substr(begins, source.find('\n', at) - begins);
    if (line.find("WhyNot_") == std::string::npos && line.find("Log::") == std::string::npos) {
      continue;
    }
    ++kept;
  }
  for (size_t at = source.find("return std::unexpected("); at != std::string::npos;
       at = source.find("return std::unexpected(", at + 1)) {
    ++asked;
    // the refusal that follows a device call keeps what it said: look back a few lines for a
    // WhyNot_ assignment carrying SDL_GetError()
    const size_t from = at < 400 ? 0 : at - 400;
    const std::string before = source.substr(from, at - from);
    const bool caused = before.find("SDL_GetError()") != std::string::npos &&
                        before.find("WhyNot_") != std::string::npos;
    const size_t closes = source.find(')', at);
    const std::string said = source.substr(at, closes == std::string::npos ? 60 : closes - at);
    const bool device = said.find("device gave") != std::string::npos ||
                        said.find("refused by the device") != std::string::npos ||
                        said.find("device refused") != std::string::npos;
    if (device && !caused) {
      silent.push_back("Renderer.cpp:" + std::to_string(LineOf(source, at)) +
                       " refuses with what the DEVICE said and keeps none of it");
    }
  }

  Note("refusals the render door carries", (double)asked, "refusals");
  Note("SDL errors it copies into WhyNot", (double)kept, "copies");
  for (const std::string &one : silent) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(asked >= 4, "the door refuses in more than one place, so this walk has a population");
  CHECK(silent.empty(),
        "**A FAULT THE DEVICE REPORTED KEEPS WHAT THE DEVICE SAID**: the sentence a caller "
        "reads is static text so the present path allocates nothing, and SDL_GetError() is "
        "thread-local -- so a refusal naming the device that does not copy its reason into "
        "WhyNot has dropped the only thing the caller could not have worked out for itself "
        "(board:1847, board:1853)");

  Covers("IV.31 a refusal that names the device keeps what the device said: static text reaches "
         "the caller and the reason reaches WhyNot, because SDL_GetError() does not survive the "
         "next call (board:1853)");
  return Report();
}
