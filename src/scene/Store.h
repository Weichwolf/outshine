#ifndef OUTSHINE_SCENE_STORE_H
#define OUTSHINE_SCENE_STORE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Register.h"

namespace outshine {

struct Entity {
  uint32_t Index = 0;
  uint32_t Generation = 0;

  [[nodiscard]] constexpr bool operator==(Entity other) const {
    return Index == other.Index && Generation == other.Generation;
  }
};

inline constexpr Entity kNoEntity{0xFFFFFFFFu, 0};
inline constexpr size_t kPairsPerEntity = 8;
inline constexpr size_t kTagsPerEntity = 8;
inline constexpr size_t kSeatsPerOffer = 4;

enum class Seat : uint8_t { Free, Claimed, Occupied };

class Store {
public:
  [[nodiscard]] bool Open(size_t capacity);

  [[nodiscard]] Entity Add(Role kind);
  void Remove(Entity of);
  [[nodiscard]] bool Alive(Entity of) const;
  [[nodiscard]] Role RoleOf(Entity of) const;

  [[nodiscard]] bool Give(Entity to, Tag tag);
  [[nodiscard]] bool Has(Entity of, Tag tag) const;

  [[nodiscard]] bool Offer(Entity at, Tag activity, size_t seats);
  [[nodiscard]] size_t Offering(Tag activity, Entity into[], size_t room) const;
  [[nodiscard]] bool Claim(Entity by, Entity at);
  [[nodiscard]] bool Use(Entity by, Entity at);
  [[nodiscard]] bool Release(Entity by, Entity at);
  [[nodiscard]] Seat SeatOf(Entity by, Entity at) const;

  [[nodiscard]] bool Link(Entity from, Relation how, Entity to);
  [[nodiscard]] Entity TargetOf(Entity of, Relation how) const;
  [[nodiscard]] size_t Targets(Entity of, Relation how, Entity into[], size_t room) const;

  [[nodiscard]] Entity Instantiate(Entity prefab);
  [[nodiscard]] Entity CopyOf(Entity instance, Entity prefabChild) const;

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  struct Pair {
    Relation How = kNoRelation;
    Entity To = kNoEntity;
  };
  struct Taken {
    Entity By = kNoEntity;
    Seat State = Seat::Free;
  };
  struct Slot {
    uint32_t Generation = 0;
    bool Held = false;
    Role Is = Role::Body;
    Tag Offers{};
    size_t SeatCount = 0;
    Taken Seats[kSeatsPerOffer] = {};
    Tag Given[kTagsPerEntity] = {};
    size_t GivenCount = 0;
    Pair Pairs[kPairsPerEntity] = {};
    size_t PairCount = 0;
  };

  [[nodiscard]] const Slot *Held(Entity of) const;
  [[nodiscard]] bool Refuse(const std::string &why);

  std::vector<Slot> Slots_;
  std::vector<uint32_t> Free_;
  std::string Error_;
};

} // namespace outshine

#endif
