#ifndef OUTSHINE_ASSEMBLED_H
#define OUTSHINE_ASSEMBLED_H

#include <vector>

#include <outshine/Store.h>

namespace outshine {

struct Assembled {
  std::vector<Entity> Bodies;
  Entity PlayerBody = kNoEntity;
  Entity PlayerMind = kNoEntity;
  Entity Nav = kNoEntity;
  Entity Assignment = kNoEntity;
};

}

#endif
