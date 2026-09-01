#include <Scene.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace outshine {

struct Scene::Kept {
  bool open(size_t capacity);
  Entity addEntity(Role role);
  void remove(Entity of);
  bool alive(Entity of) const;
  Role roleOf(Entity of) const;
  bool giveTag(Entity to, Tag tag);
  bool hasTag(Entity of, Tag tag) const;
  bool link(Entity from, Relation how, Entity to);
  bool relink(Entity from, Relation how, Entity to);
  Entity targetOf(Entity of, Relation how) const;
  size_t targets(Entity of, Relation how, Entity into[], size_t room) const;
  size_t sources(Entity to, Relation how, Entity into[], size_t room) const;
  size_t entitiesWithRole(Role role, Entity into[], size_t room) const;
  size_t linkedPairs(Relation how, Entity from[], Entity to[], size_t room) const;
  size_t entitiesWithTagAndRole(Tag tag, Role role, Entity into[], size_t room) const;
  Entity instantiate(Entity prefab);
  Entity copyOf(Entity instance, Entity prefabChild) const;
  bool offerSeats(Entity at, Tag activity, size_t seats);
  size_t entitiesOffering(Tag activity, Entity into[], size_t room) const;
  bool claimSeat(Entity by, Entity at);
  bool takeSeat(Entity by, Entity at);
  bool releaseSeat(Entity by, Entity at);
  Seat seatOf(Entity by, Entity at) const;

  size_t capacity() const { return Slots_.size(); }

  std::string_view error() const { return Said_; }

  size_t touched() const { return Touched_; }

  void resetTouched() { Touched_ = 0; }

  void Fell(Entity of);
  static constexpr uint32_t kNoRef = 0xFFFFFFFFu;

  template <size_t N> [[nodiscard]] static constexpr std::array<uint32_t, N> NoRefs() {
    std::array<uint32_t, N> filled{};
    filled.fill(kNoRef);
    return filled;
  }

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
    std::array<uint32_t, kRelations> InHead = NoRefs<kRelations>();
    uint32_t RoleNext = kNoRef, RolePrev = kNoRef;
    uint32_t OfferNext = kNoRef, OfferPrev = kNoRef;
  };

  [[nodiscard]] const Slot *Held(Entity of) const;
  [[nodiscard]] bool Refuse(std::string why);
  [[nodiscard]] bool Refuse(const char *why) noexcept;
  [[nodiscard]] bool Refuse(std::initializer_list<std::string_view> parts);
  [[nodiscard]] bool Permit(Relation how, Entity from, Entity to, bool retarget);

  [[nodiscard]] Pair &At(uint32_t ref) {
    return Slots_[ref / kPairsPerEntity].Pairs[ref % kPairsPerEntity];
  }

  [[nodiscard]] const Pair &At(uint32_t ref) const {
    return Slots_[ref / kPairsPerEntity].Pairs[ref % kPairsPerEntity];
  }

  void LinkIn(uint32_t ref);
  void UnlinkIn(uint32_t ref);
  void ErasePair(uint32_t slot, size_t pair);

  std::vector<Slot> Slots_;
  std::vector<uint32_t> Free_;
  std::array<uint32_t, kRoles> RoleHead_ = NoRefs<kRoles>();
  uint32_t OfferHead_ = kNoRef;
  std::array<uint32_t, kRelations> RelHead_ = NoRefs<kRelations>();
  static constexpr size_t kMostRefusalBytes = 256;

  struct Standing {
    Entity Source;
    Entity Under;
  };

  std::vector<Entity> Felling_;
  std::vector<Standing> Raising_;
  std::string ErrorText_;
  std::string_view Said_;
  mutable size_t Touched_ = 0;
};

namespace {

const char *Named(Role role) {
  switch (role) {
    case Role::Body: return "a body";
    case Role::Mind: return "a mind";
    case Role::Tool: return "a tool";
    case Role::Assignment: return "an assignment";
  }
  return "nothing";
}

const char *Named(Relation how) {
  switch (how) {
    case Relation::IsA: return "IsA";
    case Relation::ChildOf: return "ChildOf";
    case Relation::DrivenBy: return "DrivenBy";
    case Relation::Uses: return "Uses";
    case Relation::Assigned: return "Assigned";
    case Relation::HeldBy: return "HeldBy";
  }
  return "no relation";
}

} // namespace

bool Scene::Kept::open(size_t capacity) {
  if (capacity == 0) { return Refuse("a store of no entities holds nothing"); }
  Slots_.assign(capacity, Slot{});
  Free_.clear();
  Free_.reserve(capacity);
  Felling_.clear();
  Felling_.reserve(capacity * kOwnedRelations);
  Raising_.clear();
  Raising_.reserve(capacity);
  ErrorText_.reserve(kMostRefusalBytes);
  for (size_t at = capacity; at > 0; --at) { Free_.push_back(static_cast<uint32_t>(at - 1)); }
  for (size_t role = 0; role < kRoles; ++role) { RoleHead_[role] = kNoRef; }
  for (size_t how = 0; how < kRelations; ++how) { RelHead_[how] = kNoRef; }
  OfferHead_ = kNoRef;
  Said_ = {};
  ErrorText_.clear();
  Touched_ = 0;
  return true;
}

Entity Scene::Kept::addEntity(Role role) {
  if (Free_.empty()) {
    (void)Refuse("the store is full, and a pool refuses rather than grows");
    return kNoEntity;
  }
  const uint32_t index = Free_.back();
  Free_.pop_back();
  Slot &slot = Slots_[index];
  const uint32_t generation = slot.Generation;
  slot = Slot{};
  slot.Generation = generation;
  slot.Held = true;
  slot.Is = role;
  slot.RoleNext = RoleHead_[static_cast<size_t>(role)];
  if (slot.RoleNext != kNoRef) { Slots_[slot.RoleNext].RolePrev = index; }
  RoleHead_[static_cast<size_t>(role)] = index;
  return Entity{.Index = index, .Generation = generation};
}

void Scene::Kept::remove(Entity of) {
  if (Held(of) == nullptr) { return; }
  Felling_.clear();
  Felling_.push_back(of);
  while (!Felling_.empty()) {
    const Entity next = Felling_.back();
    Slot *held = const_cast<Slot *>(Held(next));
    if (held == nullptr) {
      Felling_.pop_back();
      continue;
    }
    bool deferred = false;
    for (size_t how = 0; how < kRelations; ++how) {
      if (!RuleOf(static_cast<Relation>(how)).OwnedByTarget) { continue; }
      for (uint32_t in = held->InHead[how]; in != kNoRef; in = At(in).InNext) {
        ++Touched_;
        const uint32_t source = in / kPairsPerEntity;
        Felling_.push_back(Entity{.Index = source, .Generation = Slots_[source].Generation});
        deferred = true;
      }
    }
    if (deferred) { continue; }
    Felling_.pop_back();
    Fell(next);
  }
}

void Scene::Kept::Fell(Entity of) {
  Slot *slot = const_cast<Slot *>(Held(of));
  if (slot == nullptr) { return; }
  const uint32_t index = of.Index;

  while (slot->PairCount > 0) { ErasePair(index, slot->PairCount - 1); }

  for (size_t how = 0; how < kRelations; ++how) {
    for (uint32_t in = slot->InHead[how]; in != kNoRef; in = slot->InHead[how]) {
      ++Touched_;
      const uint32_t source = in / kPairsPerEntity;
      ErasePair(source, in % kPairsPerEntity);
    }
  }

  if (slot->RolePrev != kNoRef) { Slots_[slot->RolePrev].RoleNext = slot->RoleNext; }
  if (slot->RoleNext != kNoRef) { Slots_[slot->RoleNext].RolePrev = slot->RolePrev; }
  if (RoleHead_[static_cast<size_t>(slot->Is)] == index) {
    RoleHead_[static_cast<size_t>(slot->Is)] = slot->RoleNext;
  }

  if (slot->Offers.value() != 0) {
    if (slot->OfferPrev != kNoRef) { Slots_[slot->OfferPrev].OfferNext = slot->OfferNext; }
    if (slot->OfferNext != kNoRef) { Slots_[slot->OfferNext].OfferPrev = slot->OfferPrev; }
    if (OfferHead_ == index) { OfferHead_ = slot->OfferNext; }
  }

  slot->Held = false;
  slot->Generation += 1;
  Free_.push_back(index);
}

bool Scene::Kept::alive(Entity of) const {
  return Held(of) != nullptr;
}

Role Scene::Kept::roleOf(Entity of) const {
  const Slot *slot = Held(of);
  return slot == nullptr ? Role::Body : slot->Is;
}

bool Scene::Kept::giveTag(Entity to, Tag tag) {
  Slot *slot = const_cast<Slot *>(Held(to));
  if (slot == nullptr) { return Refuse("a tag cannot be given to what does not stand"); }
  if (tag.value() == 0) { return Refuse("the empty tag is not in the catalogue"); }
  for (size_t at = 0; at < slot->GivenCount; ++at) {
    if (slot->Given[at] == tag) {
      return Refuse("this entity already carries that tag, and a duplicate would eat a place");
    }
  }
  if (slot->GivenCount == kTagsPerEntity) {
    return Refuse("this entity carries all the tags it can");
  }
  slot->Given[slot->GivenCount++] = tag;
  return true;
}

bool Scene::Kept::hasTag(Entity of, Tag tag) const {
  const Slot *slot = Held(of);
  size_t walked = 0;
  while (slot != nullptr && walked < Slots_.size()) {
    ++Touched_;
    for (size_t at = 0; at < slot->GivenCount; ++at) {
      if (slot->Given[at].within(tag)) { return true; }
    }
    const Slot *base = nullptr;
    for (size_t at = 0; at < slot->PairCount; ++at) {
      if (slot->Pairs[at].How == Relation::IsA) { base = Held(slot->Pairs[at].To); }
    }
    slot = base;
    ++walked;
  }
  return false;
}

void Scene::Kept::LinkIn(uint32_t ref) {
  Pair &pair = At(ref);
  Slot &target = Slots_[pair.To.Index];
  const size_t how = static_cast<size_t>(pair.How);
  pair.InNext = target.InHead[how];
  pair.InPrev = kNoRef;
  if (pair.InNext != kNoRef) { At(pair.InNext).InPrev = ref; }
  target.InHead[how] = ref;
  pair.RelNext = RelHead_[how];
  pair.RelPrev = kNoRef;
  if (pair.RelNext != kNoRef) { At(pair.RelNext).RelPrev = ref; }
  RelHead_[how] = ref;
}

void Scene::Kept::UnlinkIn(uint32_t ref) {
  Pair &pair = At(ref);
  const size_t how = static_cast<size_t>(pair.How);
  if (pair.InPrev != kNoRef) { At(pair.InPrev).InNext = pair.InNext; }
  if (pair.InNext != kNoRef) { At(pair.InNext).InPrev = pair.InPrev; }
  if (pair.To.Index < Slots_.size() && Slots_[pair.To.Index].InHead[how] == ref) {
    Slots_[pair.To.Index].InHead[how] = pair.InNext;
  }
  if (pair.RelPrev != kNoRef) { At(pair.RelPrev).RelNext = pair.RelNext; }
  if (pair.RelNext != kNoRef) { At(pair.RelNext).RelPrev = pair.RelPrev; }
  if (RelHead_[how] == ref) { RelHead_[how] = pair.RelNext; }
  pair.InNext = pair.InPrev = pair.RelNext = pair.RelPrev = kNoRef;
}

void Scene::Kept::ErasePair(uint32_t slot, size_t pair) {
  Slot &holder = Slots_[slot];
  const uint32_t ref = slot * static_cast<uint32_t>(kPairsPerEntity) + static_cast<uint32_t>(pair);
  UnlinkIn(ref);
  const size_t last = holder.PairCount - 1;
  if (pair != last) {
    const uint32_t lastRef =
        slot * static_cast<uint32_t>(kPairsPerEntity) + static_cast<uint32_t>(last);
    UnlinkIn(lastRef);
    holder.Pairs[pair] = holder.Pairs[last];
    holder.Pairs[last] = Pair{};
    LinkIn(ref);
  } else {
    holder.Pairs[last] = Pair{};
  }
  holder.PairCount = last;
}

bool Scene::Kept::Permit(Relation how, Entity from, Entity to, bool retarget) {
  const RelationRule &rule = RuleOf(how);
  const Slot *source = Held(from);
  const Slot *target = Held(to);
  if (source == nullptr || target == nullptr) {
    return Refuse({Named(how), " needs both of its ends standing"});
  }
  if ((rule.TargetRoles & RoleBit(target->Is)) == 0) {
    return Refuse({Named(how), " does not reach ", Named(target->Is)});
  }
  if (rule.SameRole && source->Is != target->Is) {
    return Refuse({Named(how), " joins likes: ", Named(source->Is), " is not ", Named(target->Is)});
  }
  if (rule.Exclusive && !retarget && !(targetOf(from, how) == kNoEntity)) {
    return Refuse({Named(how), " is exclusive, and this source already has its target"});
  }
  if (rule.SourceDoes.value() != 0 && !hasTag(from, rule.SourceDoes)) {
    return Refuse({Named(how), " asks the source to do something, and it does nothing"});
  }
  if (rule.Requires != kNoRelation && targets(from, rule.Requires, nullptr, 0) == 0) {
    return Refuse({Named(how),
                   " stands only on ",
                   Named(rule.Requires),
                   ", which this source does not have"});
  }
  if (rule.Acyclic) {
    Entity walked = to;
    for (size_t steps = 0; steps < Slots_.size() && !(walked == kNoEntity); ++steps) {
      if (walked == from) { return Refuse({Named(how), " may not close a loop"}); }
      walked = targetOf(walked, how);
    }
  }
  return true;
}

bool Scene::Kept::relink(Entity from, Relation how, Entity to) {
  if (!RuleOf(how).Exclusive) {
    return Refuse({Named(how),
                   " holds many targets, and relink is the exclusive "
                   "relation's verb"});
  }
  if (!Permit(how, from, to, true)) { return false; }
  const Slot *source = Held(from);
  size_t held = kPairsPerEntity;
  for (size_t at = 0; at < source->PairCount; ++at) {
    if (source->Pairs[at].How == how) {
      held = at;
      break;
    }
  }
  if (held == kPairsPerEntity) {
    return Refuse({Named(how), " holds nothing to relink -- taking an empty seat is Link"});
  }
  if (source->Pairs[held].To == to) { return true; }
  const uint32_t ref =
      from.Index * static_cast<uint32_t>(kPairsPerEntity) + static_cast<uint32_t>(held);
  UnlinkIn(ref);
  At(ref).To = to;
  LinkIn(ref);
  return true;
}

bool Scene::Kept::link(Entity from, Relation how, Entity to) {
  if (!Permit(how, from, to, false)) { return false; }
  Slot *writable = const_cast<Slot *>(Held(from));
  if (writable->PairCount == kPairsPerEntity) {
    return Refuse("this entity carries all the connections it can");
  }
  const uint32_t ref = from.Index * static_cast<uint32_t>(kPairsPerEntity) +
                       static_cast<uint32_t>(writable->PairCount);
  writable->Pairs[writable->PairCount++] = Pair{.How = how,
                                                .To = to,
                                                .InNext = kNoRef,
                                                .InPrev = kNoRef,
                                                .RelNext = kNoRef,
                                                .RelPrev = kNoRef};
  LinkIn(ref);
  return true;
}

Entity Scene::Kept::targetOf(Entity of, Relation how) const {
  const Slot *slot = Held(of);
  if (slot == nullptr) { return kNoEntity; }
  for (size_t at = 0; at < slot->PairCount; ++at) {
    ++Touched_;
    if (slot->Pairs[at].How == how) { return slot->Pairs[at].To; }
  }
  return kNoEntity;
}

size_t Scene::Kept::targets(Entity of, Relation how, Entity into[], size_t room) const {
  const Slot *slot = Held(of);
  if (slot == nullptr) { return 0; }
  size_t found = 0;
  for (size_t at = 0; at < slot->PairCount; ++at) {
    ++Touched_;
    if (slot->Pairs[at].How != how) { continue; }
    if (into != nullptr && found < room) { into[found] = slot->Pairs[at].To; }
    ++found;
  }
  return found;
}

size_t Scene::Kept::sources(Entity to, Relation how, Entity into[], size_t room) const {
  const Slot *slot = Held(to);
  if (slot == nullptr) { return 0; }
  size_t found = 0;
  for (uint32_t in = slot->InHead[static_cast<size_t>(how)]; in != kNoRef; in = At(in).InNext) {
    ++Touched_;
    const uint32_t source = in / kPairsPerEntity;
    if (into != nullptr && found < room) {
      into[found] = Entity{.Index = source, .Generation = Slots_[source].Generation};
    }
    ++found;
  }
  return found;
}

size_t Scene::Kept::entitiesWithRole(Role role, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = RoleHead_[static_cast<size_t>(role)]; at != kNoRef; at = Slots_[at].RoleNext) {
    ++Touched_;
    if (into != nullptr && found < room) {
      into[found] = Entity{.Index = at, .Generation = Slots_[at].Generation};
    }
    ++found;
  }
  return found;
}

size_t Scene::Kept::linkedPairs(Relation how, Entity from[], Entity to[], size_t room) const {
  size_t found = 0;
  for (uint32_t ref = RelHead_[static_cast<size_t>(how)]; ref != kNoRef; ref = At(ref).RelNext) {
    ++Touched_;
    const uint32_t source = ref / kPairsPerEntity;
    if (found < room) {
      if (from != nullptr) {
        from[found] = Entity{.Index = source, .Generation = Slots_[source].Generation};
      }
      if (to != nullptr) { to[found] = At(ref).To; }
    }
    ++found;
  }
  return found;
}

size_t Scene::Kept::entitiesWithTagAndRole(Tag tag, Role role, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = RoleHead_[static_cast<size_t>(role)]; at != kNoRef; at = Slots_[at].RoleNext) {
    ++Touched_;
    const Entity one{.Index = at, .Generation = Slots_[at].Generation};
    if (!hasTag(one, tag)) { continue; }
    if (into != nullptr && found < room) { into[found] = one; }
    ++found;
  }
  return found;
}

Entity Scene::Kept::instantiate(Entity prefab) {
  const Slot *base = Held(prefab);
  if (base == nullptr) {
    (void)Refuse("only what stands can be instantiated");
    return kNoEntity;
  }
  const Entity instance = addEntity(base->Is);
  if (!alive(instance)) { return kNoEntity; }
  if (!link(instance, Relation::IsA, prefab)) {
    remove(instance);
    return kNoEntity;
  }
  Raising_.clear();
  Raising_.push_back(Standing{.Source = prefab, .Under = instance});
  while (!Raising_.empty()) {
    const Standing at = Raising_.back();
    Raising_.pop_back();
    for (uint32_t in = Slots_[at.Source.Index].InHead[static_cast<size_t>(Relation::ChildOf)];
         in != kNoRef;) {
      ++Touched_;
      const uint32_t source = in / kPairsPerEntity;
      const uint32_t next = At(in).InNext;
      const Entity childId{.Index = source, .Generation = Slots_[source].Generation};
      if (childId == instance || childId == at.Under) {
        in = next;
        continue;
      }
      const Slot *childBase = Held(childId);
      const Entity copied = childBase == nullptr ? kNoEntity : addEntity(childBase->Is);
      if (!alive(copied) || !link(copied, Relation::IsA, childId) ||
          !link(copied, Relation::ChildOf, at.Under)) {
        if (alive(copied)) { remove(copied); }
        remove(instance);
        return kNoEntity;
      }
      Raising_.push_back(Standing{.Source = childId, .Under = copied});
      in = next;
    }
  }
  return instance;
}

Entity Scene::Kept::copyOf(Entity instance, Entity prefabChild) const {
  const Slot *slot = Held(instance);
  if (slot == nullptr) { return kNoEntity; }
  for (uint32_t in = slot->InHead[static_cast<size_t>(Relation::ChildOf)]; in != kNoRef;
       in = At(in).InNext) {
    ++Touched_;
    const uint32_t source = in / kPairsPerEntity;
    const Entity childId{.Index = source, .Generation = Slots_[source].Generation};
    if (targetOf(childId, Relation::IsA) == prefabChild) { return childId; }
  }
  return kNoEntity;
}

bool Scene::Kept::offerSeats(Entity at, Tag activity, size_t seats) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (slot == nullptr) { return Refuse("an offer needs its object standing"); }
  if (activity.value() == 0) {
    return Refuse("an offer advertises an activity from the catalogue");
  }
  if (seats == 0 || seats > kSeatsPerOffer) {
    return Refuse("an offer holds between one seat and the pool's few");
  }
  if (slot->Offers.value() != 0) {
    return Refuse("this object already advertises, and one object is one offer");
  }
  slot->Offers = activity;
  slot->SeatCount = seats;
  for (size_t seat = 0; seat < seats; ++seat) { slot->Seats[seat] = Taken{}; }
  slot->OfferNext = OfferHead_;
  slot->OfferPrev = kNoRef;
  if (OfferHead_ != kNoRef) { Slots_[OfferHead_].OfferPrev = at.Index; }
  OfferHead_ = at.Index;
  return true;
}

size_t Scene::Kept::entitiesOffering(Tag activity, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = OfferHead_; at != kNoRef; at = Slots_[at].OfferNext) {
    ++Touched_;
    const Slot &slot = Slots_[at];
    if (!slot.Offers.within(activity)) { continue; }
    if (into != nullptr && found < room) {
      into[found] = Entity{.Index = at, .Generation = slot.Generation};
    }
    ++found;
  }
  return found;
}

bool Scene::Kept::claimSeat(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (Held(by) == nullptr || slot == nullptr) {
    return Refuse("a claim needs both of its ends standing");
  }
  if (slot->Offers.value() == 0) { return Refuse("this object advertises nothing to claim"); }
  if (!(seatOf(by, at) == Seat::Free)) {
    return Refuse("one claimant holds at most one seat here");
  }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.State != Seat::Free && Held(taken.By) == nullptr) { taken = Taken{}; }
    if (taken.State == Seat::Free) {
      taken = Taken{.By = by, .State = Seat::Claimed};
      return true;
    }
  }
  return Refuse("every seat of this offer is claimed or occupied -- come back or go elsewhere");
}

bool Scene::Kept::takeSeat(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (Held(by) == nullptr || slot == nullptr) {
    return Refuse("use needs both the claimant and the object standing");
  }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.By == by && taken.State == Seat::Claimed) {
      taken.State = Seat::Occupied;
      return true;
    }
  }
  return Refuse("use stands only on a claim, and this claimant holds none here");
}

bool Scene::Kept::releaseSeat(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (Held(by) == nullptr || slot == nullptr) {
    return Refuse("a release needs both the claimant and the object standing");
  }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.By == by && taken.State != Seat::Free) {
      taken = Taken{};
      return true;
    }
  }
  return Refuse("nothing of this claimant's stands here to release");
}

Seat Scene::Kept::seatOf(Entity by, Entity at) const {
  const Slot *slot = Held(at);
  if (slot == nullptr) { return Seat::Free; }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    if (slot->Seats[seat].By == by) { return slot->Seats[seat].State; }
  }
  return Seat::Free;
}

const Scene::Kept::Slot *Scene::Kept::Held(Entity of) const {
  if (of.Index >= Slots_.size()) { return nullptr; }
  const Slot &slot = Slots_[of.Index];
  if (!slot.Held || slot.Generation != of.Generation) { return nullptr; }
  return &slot;
}

bool Scene::Kept::Refuse(std::string why) {
  ErrorText_ = std::move(why);
  Said_ = ErrorText_;
  return false;
}

bool Scene::Kept::Refuse(const char *why) noexcept {
  Said_ = why;
  return false;
}

bool Scene::Kept::Refuse(std::initializer_list<std::string_view> parts) {
  ErrorText_.clear();
  for (const std::string_view part : parts) { ErrorText_ += part; }
  Said_ = ErrorText_;
  return false;
}

bool Scene::open(size_t capacity) {
  return Kept_->open(capacity);
}

Entity Scene::addEntity(Role role) {
  return Kept_->addEntity(role);
}

void Scene::remove(Entity of) {
  Kept_->remove(of);
}

bool Scene::alive(Entity of) const {
  return Kept_->alive(of);
}

Role Scene::roleOf(Entity of) const {
  return Kept_->roleOf(of);
}

bool Scene::giveTag(Entity to, Tag tag) {
  return Kept_->giveTag(to, tag);
}

bool Scene::hasTag(Entity of, Tag tag) const {
  return Kept_->hasTag(of, tag);
}

bool Scene::link(Entity from, Relation how, Entity to) {
  return Kept_->link(from, how, to);
}

bool Scene::relink(Entity from, Relation how, Entity to) {
  return Kept_->relink(from, how, to);
}

Entity Scene::targetOf(Entity of, Relation how) const {
  return Kept_->targetOf(of, how);
}

size_t Scene::targets(Entity of, Relation how, Entity into[], size_t room) const {
  return Kept_->targets(of, how, into, room);
}

size_t Scene::sources(Entity to, Relation how, Entity into[], size_t room) const {
  return Kept_->sources(to, how, into, room);
}

size_t Scene::entitiesWithRole(Role role, Entity into[], size_t room) const {
  return Kept_->entitiesWithRole(role, into, room);
}

size_t Scene::linkedPairs(Relation how, Entity from[], Entity to[], size_t room) const {
  return Kept_->linkedPairs(how, from, to, room);
}

size_t Scene::entitiesWithTagAndRole(Tag tag, Role role, Entity into[], size_t room) const {
  return Kept_->entitiesWithTagAndRole(tag, role, into, room);
}

Entity Scene::instantiate(Entity prefab) {
  return Kept_->instantiate(prefab);
}

Entity Scene::copyOf(Entity instance, Entity prefabChild) const {
  return Kept_->copyOf(instance, prefabChild);
}

bool Scene::offerSeats(Entity at, Tag activity, size_t seats) {
  return Kept_->offerSeats(at, activity, seats);
}

size_t Scene::entitiesOffering(Tag activity, Entity into[], size_t room) const {
  return Kept_->entitiesOffering(activity, into, room);
}

bool Scene::claimSeat(Entity by, Entity at) {
  return Kept_->claimSeat(by, at);
}

bool Scene::takeSeat(Entity by, Entity at) {
  return Kept_->takeSeat(by, at);
}

bool Scene::releaseSeat(Entity by, Entity at) {
  return Kept_->releaseSeat(by, at);
}

Seat Scene::seatOf(Entity by, Entity at) const {
  return Kept_->seatOf(by, at);
}

size_t Scene::capacity() const {
  return Kept_->capacity();
}

std::string_view Scene::error() const {
  return Kept_->error();
}

size_t Scene::touched() const {
  return Kept_->touched();
}

void Scene::resetTouched() {
  Kept_->resetTouched();
}

Scene::Scene() : Kept_(std::make_unique<Kept>()) {}

Scene::~Scene() = default;
Scene::Scene(Scene &&) noexcept = default;
Scene &Scene::operator=(Scene &&) noexcept = default;

} // namespace outshine
