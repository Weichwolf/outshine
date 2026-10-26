#ifndef OUTSHINE_STORAGE_CACHETRIM_H
#define OUTSHINE_STORAGE_CACHETRIM_H

#include <cstdint>

namespace outshine {

struct CacheTrimOptions {
  uint64_t BudgetBytes = 0;
  bool Apply = false;
  bool Compact = false;
};

struct CacheTrimResult {
  uint64_t StoredBytes = 0;
  uint64_t RetainedBytes = 0;
  uint64_t EntriesRemoved = 0;
  uint64_t PackagesRemoved = 0;
  uint64_t UnknownUse = 0;
  uint64_t FileBytesBefore = 0;
  uint64_t FileBytesAfter = 0;
};

}
#endif
