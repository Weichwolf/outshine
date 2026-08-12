#ifndef SHA256_H
#define SHA256_H

#include <cstddef>
#include <string>

namespace outshine {

/* FIPS 180-4 SHA-256, lower-case hex, 64 characters.
 *
 * WHY THE WHOLE 256 BITS AND NOT A TRUNCATION: the content store names a file by this digest, so two
 * distinct keys landing on one name would serve one source's bytes under another's request. At 10^12
 * distinct keys the birthday bound n^2/2^(b+1) is 10^24 / 2^257 ~ 4e-54, which is impossible rather
 * than unlikely; a 64-bit truncation at the same load is 10^24 / 2^65 ~ 3e4 — a bound above one is
 * the arithmetic saying certain, and 64 bits turn near 2^32 ~ 4e9 keys, three orders under it. */
[[nodiscard]] std::string Sha256Hex(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(const std::string &text);

} // namespace outshine
#endif
