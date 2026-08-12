/* WHICH INSTRUMENT THE RUN CARRIED, taken from the compiler's own answer and from nothing else. A
 * flag the build remembers to pass can drift from the flags it actually compiled with, and a row
 * that names the wrong instrument is worse than a row that names none: a sanitised frame
 * distribution read as a shipping one is a regression that was never there. */
#ifndef SANITISERS_H
#define SANITISERS_H

namespace outshine::Clients {

#if defined(__has_feature)
#if __has_feature(address_sanitizer) && __has_feature(undefined_behavior_sanitizer)
constexpr const char *kSanitisers = "address,undefined";
#elif __has_feature(address_sanitizer)
constexpr const char *kSanitisers = "address";
#elif __has_feature(undefined_behavior_sanitizer)
constexpr const char *kSanitisers = "undefined";
#else
constexpr const char *kSanitisers = "";
#endif
#else
constexpr const char *kSanitisers = "";
#endif

} // namespace outshine::Clients
#endif
