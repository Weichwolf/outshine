#ifndef RANK_H
#define RANK_H

#include <cstdint>

namespace outshine::Generators {

/* The order generators claim space in, declared at bring-up. No enumerators: what the ranks are is
 * a declaration, and naming them here would put a content taxonomy in the engine. */
enum class Rank : uint16_t {};

struct BodyRange {
  uint32_t First = 0, Count = 0;
};

} // namespace outshine::Generators
#endif
