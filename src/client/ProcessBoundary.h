#ifndef OUTSHINE_CLIENT_PROCESSBOUNDARY_H
#define OUTSHINE_CLIENT_PROCESSBOUNDARY_H
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <utility>

namespace outshine::Client {
namespace Says {
inline constexpr auto kFatalAllocation = "outshine-client: fatal: memory allocation failed\n";
inline constexpr auto kFatalException = "outshine-client: fatal: ";
inline constexpr auto kUnknownException = "outshine-client: fatal: unknown exception\n";
}

template <class Entry>
[[nodiscard]] int RunAtProcessBoundary(Entry &&entry, std::FILE *errors = stderr) noexcept {
  try {
    return std::forward<Entry>(entry)();
  } catch (const std::bad_alloc &) {
    std::fputs(Says::kFatalAllocation, errors);
  } catch (const std::exception &error) {
    std::fputs(Says::kFatalException, errors);
    std::fputs(error.what(), errors);
    std::fputc('\n', errors);
  } catch (...) { std::fputs(Says::kUnknownException, errors); }
  return EXIT_FAILURE;
}
}

#endif
