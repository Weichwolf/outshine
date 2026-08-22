#include "Store.h"

namespace outshine {

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
  }
  return "no relation";
}

} // namespace

bool Store::Open(size_t capacity) {
  if (capacity == 0) { return Refuse("a store of no entities holds nothing"); }
  Slots_.assign(capacity, Slot{});
  Free_.clear();
  Free_.reserve(capacity);
  for (size_t at = capacity; at > 0; --at) { Free_.push_back((uint32_t)(at - 1)); }
  for (size_t role = 0; role < kRoles; ++role) { RoleHead_[role] = kNoRef; }
  for (size_t how = 0; how < kRelations; ++how) { RelHead_[how] = kNoRef; }
  OfferHead_ = kNoRef;
  Error_.clear();
  Touched_ = 0;
  return true;
}

Entity Store::Add(Role role) {
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
  slot.RoleNext = RoleHead_[(size_t)role];
  if (slot.RoleNext != kNoRef) { Slots_[slot.RoleNext].RolePrev = index; }
  RoleHead_[(size_t)role] = index;
  return Entity{index, generation};
}

void Store::Remove(Entity of) {
  Slot *slot = const_cast<Slot *>(Held(of));
  if (slot == nullptr) { return; }
  const uint32_t index = of.Index;

  while (slot->PairCount > 0) { ErasePair(index, slot->PairCount - 1); }

  for (size_t how = 0; how < kRelations; ++how) {
    const bool owns = RuleOf((Relation)how).OwnedByTarget;
    for (uint32_t in = slot->InHead[how]; in != kNoRef; in = slot->InHead[how]) {
      ++Touched_;
      const uint32_t source = in / kPairsPerEntity;
      if (owns) {
        Remove(Entity{source, Slots_[source].Generation});
      } else {
        ErasePair(source, in % kPairsPerEntity);
      }
    }
  }

  if (slot->RolePrev != kNoRef) { Slots_[slot->RolePrev].RoleNext = slot->RoleNext; }
  if (slot->RoleNext != kNoRef) { Slots_[slot->RoleNext].RolePrev = slot->RolePrev; }
  if (RoleHead_[(size_t)slot->Is] == index) { RoleHead_[(size_t)slot->Is] = slot->RoleNext; }

  if (slot->Offers.Value() != 0) {
    if (slot->OfferPrev != kNoRef) { Slots_[slot->OfferPrev].OfferNext = slot->OfferNext; }
    if (slot->OfferNext != kNoRef) { Slots_[slot->OfferNext].OfferPrev = slot->OfferPrev; }
    if (OfferHead_ == index) { OfferHead_ = slot->OfferNext; }
  }

  slot->Held = false;
  slot->Generation += 1;
  Free_.push_back(index);
}

bool Store::Alive(Entity of) const { return Held(of) != nullptr; }

Role Store::RoleOf(Entity of) const {
  const Slot *slot = Held(of);
  return slot == nullptr ? Role::Body : slot->Is;
}

bool Store::Give(Entity to, Tag tag) {
  Slot *slot = const_cast<Slot *>(Held(to));
  if (slot == nullptr) { return Refuse("a tag cannot be given to what does not stand"); }
  if (tag.Value() == 0) { return Refuse("the empty tag is not in the catalogue"); }
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

bool Store::Has(Entity of, Tag tag) const {
  const Slot *slot = Held(of);
  size_t walked = 0;
  while (slot != nullptr && walked < Slots_.size()) {
    ++Touched_;
    for (size_t at = 0; at < slot->GivenCount; ++at) {
      if (slot->Given[at].Within(tag)) { return true; }
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

void Store::LinkIn(uint32_t ref) {
  Pair &pair = At(ref);
  Slot &target = Slots_[pair.To.Index];
  const size_t how = (size_t)pair.How;
  pair.InNext = target.InHead[how];
  pair.InPrev = kNoRef;
  if (pair.InNext != kNoRef) { At(pair.InNext).InPrev = ref; }
  target.InHead[how] = ref;
  pair.RelNext = RelHead_[how];
  pair.RelPrev = kNoRef;
  if (pair.RelNext != kNoRef) { At(pair.RelNext).RelPrev = ref; }
  RelHead_[how] = ref;
}

void Store::UnlinkIn(uint32_t ref) {
  Pair &pair = At(ref);
  const size_t how = (size_t)pair.How;
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

void Store::ErasePair(uint32_t slot, size_t pair) {
  Slot &holder = Slots_[slot];
  const uint32_t ref = slot * (uint32_t)kPairsPerEntity + (uint32_t)pair;
  UnlinkIn(ref);
  const size_t last = holder.PairCount - 1;
  if (pair != last) {
    const uint32_t lastRef = slot * (uint32_t)kPairsPerEntity + (uint32_t)last;
    UnlinkIn(lastRef);
    holder.Pairs[pair] = holder.Pairs[last];
    holder.Pairs[last] = Pair{};
    LinkIn(ref);
  } else {
    holder.Pairs[last] = Pair{};
  }
  holder.PairCount = last;
}

bool Store::Relink(Entity from, Relation how, Entity to) {
  const RelationRule &rule = RuleOf(how);
  if (!rule.Exclusive) {
    return Refuse(std::string(Named(how)) +
                  " holds many targets, and relink is the exclusive relation's verb");
  }
  const Slot *source = Held(from);
  const Slot *target = Held(to);
  if (source == nullptr || target == nullptr) {
    return Refuse(std::string(Named(how)) + " needs both of its ends standing");
  }
  size_t held = kPairsPerEntity;
  for (size_t at = 0; at < source->PairCount; ++at) {
    if (source->Pairs[at].How == how) {
      held = at;
      break;
    }
  }
  if (held == kPairsPerEntity) {
    return Refuse(std::string(Named(how)) +
                  " holds nothing to relink -- taking an empty seat is Link");
  }
  if (source->Pairs[held].To == to) { return true; }
  if ((rule.TargetRoles & RoleBit(target->Is)) == 0) {
    return Refuse(std::string(Named(how)) + " does not reach " + Named(target->Is));
  }
  if (rule.SameRole && source->Is != target->Is) {
    return Refuse(std::string(Named(how)) + " joins likes: " + Named(source->Is) + " is not " +
                  Named(target->Is));
  }
  if (rule.Acyclic) {
    Entity walked = to;
    for (size_t steps = 0; steps < Slots_.size() && !(walked == kNoEntity); ++steps) {
      if (walked == from) {
        return Refuse(std::string(Named(how)) + " may not close a loop");
      }
      walked = TargetOf(walked, how);
    }
  }
  const uint32_t ref = from.Index * (uint32_t)kPairsPerEntity + (uint32_t)held;
  UnlinkIn(ref);
  At(ref).To = to;
  LinkIn(ref);
  return true;
}

bool Store::Link(Entity from, Relation how, Entity to) {
  const Slot *source = Held(from);
  const Slot *target = Held(to);
  const RelationRule &rule = RuleOf(how);
  if (source == nullptr || target == nullptr) {
    return Refuse(std::string(Named(how)) + " needs both of its ends standing");
  }
  if ((rule.TargetRoles & RoleBit(target->Is)) == 0) {
    return Refuse(std::string(Named(how)) + " does not reach " + Named(target->Is));
  }
  if (rule.SameRole && source->Is != target->Is) {
    return Refuse(std::string(Named(how)) + " joins likes: " + Named(source->Is) + " is not " +
                  Named(target->Is));
  }
  if (rule.Exclusive && !(TargetOf(from, how) == kNoEntity)) {
    return Refuse(std::string(Named(how)) + " is exclusive, and this source already has its target");
  }
  if (rule.SourceDoes.Value() != 0 && !Has(from, rule.SourceDoes)) {
    return Refuse(std::string(Named(how)) + " asks the source to do something, and it does nothing");
  }
  if (rule.Requires != kNoRelation && Targets(from, rule.Requires, nullptr, 0) == 0) {
    return Refuse(std::string(Named(how)) + " stands only on " + Named(rule.Requires) +
                  ", which this source does not have");
  }
  if (rule.Acyclic) {
    Entity walked = to;
    for (size_t steps = 0; steps < Slots_.size() && !(walked == kNoEntity); ++steps) {
      if (walked == from) {
        return Refuse(std::string(Named(how)) + " may not close a loop");
      }
      walked = TargetOf(walked, how);
    }
  }
  Slot *writable = const_cast<Slot *>(source);
  if (writable->PairCount == kPairsPerEntity) {
    return Refuse("this entity carries all the connections it can");
  }
  const uint32_t ref = from.Index * (uint32_t)kPairsPerEntity + (uint32_t)writable->PairCount;
  writable->Pairs[writable->PairCount++] = Pair{how, to, kNoRef, kNoRef, kNoRef, kNoRef};
  LinkIn(ref);
  return true;
}

Entity Store::TargetOf(Entity of, Relation how) const {
  const Slot *slot = Held(of);
  if (slot == nullptr) { return kNoEntity; }
  for (size_t at = 0; at < slot->PairCount; ++at) {
    ++Touched_;
    if (slot->Pairs[at].How == how) { return slot->Pairs[at].To; }
  }
  return kNoEntity;
}

size_t Store::Targets(Entity of, Relation how, Entity into[], size_t room) const {
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

size_t Store::Sources(Entity to, Relation how, Entity into[], size_t room) const {
  const Slot *slot = Held(to);
  if (slot == nullptr) { return 0; }
  size_t found = 0;
  for (uint32_t in = slot->InHead[(size_t)how]; in != kNoRef; in = At(in).InNext) {
    ++Touched_;
    const uint32_t source = in / kPairsPerEntity;
    if (into != nullptr && found < room) { into[found] = Entity{source, Slots_[source].Generation}; }
    ++found;
  }
  return found;
}

size_t Store::Cast(Role role, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = RoleHead_[(size_t)role]; at != kNoRef; at = Slots_[at].RoleNext) {
    ++Touched_;
    if (into != nullptr && found < room) { into[found] = Entity{at, Slots_[at].Generation}; }
    ++found;
  }
  return found;
}

size_t Store::Pairs(Relation how, Entity from[], Entity to[], size_t room) const {
  size_t found = 0;
  for (uint32_t ref = RelHead_[(size_t)how]; ref != kNoRef; ref = At(ref).RelNext) {
    ++Touched_;
    const uint32_t source = ref / kPairsPerEntity;
    if (found < room) {
      if (from != nullptr) { from[found] = Entity{source, Slots_[source].Generation}; }
      if (to != nullptr) { to[found] = At(ref).To; }
    }
    ++found;
  }
  return found;
}

size_t Store::Bearing(Tag tag, Role role, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = RoleHead_[(size_t)role]; at != kNoRef; at = Slots_[at].RoleNext) {
    ++Touched_;
    const Entity one{at, Slots_[at].Generation};
    if (!Has(one, tag)) { continue; }
    if (into != nullptr && found < room) { into[found] = one; }
    ++found;
  }
  return found;
}

Entity Store::Instantiate(Entity prefab) {
  const Slot *base = Held(prefab);
  if (base == nullptr) {
    (void)Refuse("only what stands can be instantiated");
    return kNoEntity;
  }
  const Entity instance = Add(base->Is);
  if (!Alive(instance)) { return kNoEntity; }
  if (!Link(instance, Relation::IsA, prefab)) {
    Remove(instance);
    return kNoEntity;
  }
  for (uint32_t in = Slots_[prefab.Index].InHead[(size_t)Relation::ChildOf]; in != kNoRef;) {
    ++Touched_;
    const uint32_t source = in / kPairsPerEntity;
    const uint32_t next = At(in).InNext;
    const Entity childId{source, Slots_[source].Generation};
    if (!(childId == instance)) {
      const Entity copied = Instantiate(childId);
      if (!Alive(copied) || !Link(copied, Relation::ChildOf, instance)) {
        const std::string why = Error_;
        if (Alive(copied)) { Remove(copied); }
        Remove(instance);
        Error_ = why;
        return kNoEntity;
      }
    }
    in = next;
  }
  return instance;
}

Entity Store::CopyOf(Entity instance, Entity prefabChild) const {
  const Slot *slot = Held(instance);
  if (slot == nullptr) { return kNoEntity; }
  for (uint32_t in = slot->InHead[(size_t)Relation::ChildOf]; in != kNoRef; in = At(in).InNext) {
    ++Touched_;
    const uint32_t source = in / kPairsPerEntity;
    const Entity childId{source, Slots_[source].Generation};
    if (TargetOf(childId, Relation::IsA) == prefabChild) { return childId; }
  }
  return kNoEntity;
}

bool Store::Offer(Entity at, Tag activity, size_t seats) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (slot == nullptr) { return Refuse("an offer needs its object standing"); }
  if (activity.Value() == 0) { return Refuse("an offer advertises an activity from the catalogue"); }
  if (seats == 0 || seats > kSeatsPerOffer) {
    return Refuse("an offer holds between one seat and the pool's few");
  }
  if (slot->Offers.Value() != 0) { return Refuse("this object already advertises, and one object is one offer"); }
  slot->Offers = activity;
  slot->SeatCount = seats;
  for (size_t seat = 0; seat < seats; ++seat) { slot->Seats[seat] = Taken{}; }
  slot->OfferNext = OfferHead_;
  slot->OfferPrev = kNoRef;
  if (OfferHead_ != kNoRef) { Slots_[OfferHead_].OfferPrev = at.Index; }
  OfferHead_ = at.Index;
  return true;
}

size_t Store::Offering(Tag activity, Entity into[], size_t room) const {
  size_t found = 0;
  for (uint32_t at = OfferHead_; at != kNoRef; at = Slots_[at].OfferNext) {
    ++Touched_;
    const Slot &slot = Slots_[at];
    if (!slot.Offers.Within(activity)) { continue; }
    if (into != nullptr && found < room) { into[found] = Entity{at, slot.Generation}; }
    ++found;
  }
  return found;
}

bool Store::Claim(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (Held(by) == nullptr || slot == nullptr) { return Refuse("a claim needs both of its ends standing"); }
  if (slot->Offers.Value() == 0) { return Refuse("this object advertises nothing to claim"); }
  if (!(SeatOf(by, at) == Seat::Free)) { return Refuse("one claimant holds at most one seat here"); }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.State != Seat::Free && Held(taken.By) == nullptr) { taken = Taken{}; }
    if (taken.State == Seat::Free) {
      taken = Taken{by, Seat::Claimed};
      return true;
    }
  }
  return Refuse("every seat of this offer is claimed or occupied -- come back or go elsewhere");
}

bool Store::Use(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (slot == nullptr) { return Refuse("what is used must stand"); }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.By == by && taken.State == Seat::Claimed) {
      taken.State = Seat::Occupied;
      return true;
    }
  }
  return Refuse("use stands only on a claim, and this claimant holds none here");
}

bool Store::Release(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (slot == nullptr) { return Refuse("what is released must stand"); }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    Taken &taken = slot->Seats[seat];
    if (taken.By == by && taken.State != Seat::Free) {
      taken = Taken{};
      return true;
    }
  }
  return Refuse("nothing of this claimant's stands here to release");
}

Seat Store::SeatOf(Entity by, Entity at) const {
  const Slot *slot = Held(at);
  if (slot == nullptr) { return Seat::Free; }
  for (size_t seat = 0; seat < slot->SeatCount; ++seat) {
    if (slot->Seats[seat].By == by) { return slot->Seats[seat].State; }
  }
  return Seat::Free;
}

const Store::Slot *Store::Held(Entity of) const {
  if (of.Index >= Slots_.size()) { return nullptr; }
  const Slot &slot = Slots_[of.Index];
  if (!slot.Held || slot.Generation != of.Generation) { return nullptr; }
  return &slot;
}

bool Store::Refuse(const std::string &why) {
  Error_ = why;
  return false;
}

} // namespace outshine
