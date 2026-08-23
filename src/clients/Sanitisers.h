#ifndef OUTSHINE_CLIENTS_SANITISERS_H
#define OUTSHINE_CLIENTS_SANITISERS_H

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

}
#endif
