#ifndef OUTSHINE_WORLD_ENTITY_ENTITYBODYSTATE_H
#define OUTSHINE_WORLD_ENTITY_ENTITYBODYSTATE_H

#include "Entity.h"
#include "RigidBodyState.h"

namespace outshine::World {

struct EntityBodyState {
  Entity Owner = kNoEntity;
  RigidBodyState Motion;
};

}

#endif
