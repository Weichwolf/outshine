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

  [[nodiscard]] Entity Add(Role role);
  void Remove(Entity of);
  [[nodiscard]] bool Alive(Entity of) const;
  [[nodiscard]] Role RoleOf(Entity of) const;

  [[nodiscard]] bool Give(Entity to, Tag tag);
  [[nodiscard]] bool Has(Entity of, Tag tag) const;

  [[nodiscard]] bool Link(Entity from, Relation how, Entity to);
  [[nodiscard]] bool Relink(Entity from, Relation how, Entity to);
  [[nodiscard]] Entity TargetOf(Entity of, Relation how) const;
  [[nodiscard]] size_t Targets(Entity of, Relation how, Entity into[], size_t room) const;

  [[nodiscard]] size_t Sources(Entity to, Relation how, Entity into[], size_t room) const;
  [[nodiscard]] size_t Cast(Role role, Entity into[], size_t room) const;
  [[nodiscard]] size_t Pairs(Relation how, Entity from[], Entity to[], size_t room) const;
  [[nodiscard]] size_t Bearing(Tag tag, Role role, Entity into[], size_t room) const;

  [[nodiscard]] Entity Instantiate(Entity prefab);
  [[nodiscard]] Entity CopyOf(Entity instance, Entity prefabChild) const;

  [[nodiscard]] bool Offer(Entity at, Tag activity, size_t seats);
  [[nodiscard]] size_t Offering(Tag activity, Entity into[], size_t room) const;
  [[nodiscard]] bool Claim(Entity by, Entity at);
  [[nodiscard]] bool Use(Entity by, Entity at);
  [[nodiscard]] bool Release(Entity by, Entity at);
  [[nodiscard]] Seat SeatOf(Entity by, Entity at) const;

  [[nodiscard]] size_t Capacity() const { return Slots_.size(); }
  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] size_t Touched() const { return Touched_; }
  void ResetTouched() { Touched_ = 0; }

private:
  static constexpr uint32_t kNoRef = 0xFFFFFFFFu;

  struct Taken {
    Entity By = kNoEntity;
    Seat State = Seat::Free;
  };
  struct Pair {
    Relation How = kNoRelation;
    Entity To = kNoEntity;
    uint32_t InNext = kNoRef, InPrev = kNoRef;
    uint32_t RelNext = kNoRef, RelPrev = kNoRef;
  };
  struct Slot {
    uint32_t Generation = 0;
    bool Held = false;
    Role Is = Role::Body;
    Tag Given[kTagsPerEntity] = {};
    size_t GivenCount = 0;
    Pair Pairs[kPairsPerEntity] = {};
    size_t PairCount = 0;
    Tag Offers{};
    size_t SeatCount = 0;
    Taken Seats[kSeatsPerOffer] = {};
    uint32_t InHead[kRelations] = {kNoRef, kNoRef, kNoRef, kNoRef, kNoRef};
    uint32_t RoleNext = kNoRef, RolePrev = kNoRef;
    uint32_t OfferNext = kNoRef, OfferPrev = kNoRef;
  };

  [[nodiscard]] const Slot *Held(Entity of) const;
  [[nodiscard]] bool Refuse(const std::string &why);

  [[nodiscard]] Pair &At(uint32_t ref) { return Slots_[ref / kPairsPerEntity].Pairs[ref % kPairsPerEntity]; }
  [[nodiscard]] const Pair &At(uint32_t ref) const {
    return Slots_[ref / kPairsPerEntity].Pairs[ref % kPairsPerEntity];
  }
  void LinkIn(uint32_t ref);
  void UnlinkIn(uint32_t ref);
  void ErasePair(uint32_t slot, size_t pair);

  std::vector<Slot> Slots_;
  std::vector<uint32_t> Free_;
  uint32_t RoleHead_[kRoles] = {kNoRef, kNoRef, kNoRef, kNoRef};
  uint32_t OfferHead_ = kNoRef;
  uint32_t RelHead_[kRelations] = {kNoRef, kNoRef, kNoRef, kNoRef, kNoRef};
  std::string Error_;
  mutable size_t Touched_ = 0;
};

} // namespace outshine

#endif
