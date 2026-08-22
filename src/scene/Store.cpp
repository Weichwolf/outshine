#include "Store.h"

namespace outshine {

namespace {

const char *Named(Role kind) {
  switch (kind) {
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
  Error_.clear();
  return true;
}

Entity Store::Add(Role kind) {
  if (Free_.empty()) {
    (void)Refuse("the store is full, and a pool refuses rather than grows");
    return kNoEntity;
  }
  const uint32_t index = Free_.back();
  Free_.pop_back();
  Slot &slot = Slots_[index];
  slot.Held = true;
  slot.Is = kind;
  slot.GivenCount = 0;
  slot.PairCount = 0;
  return Entity{index, slot.Generation};
}

void Store::Remove(Entity of) {
  Slot *slot = const_cast<Slot *>(Held(of));
  if (slot == nullptr) { return; }
  slot->Held = false;
  slot->Generation += 1;
  Free_.push_back(of.Index);
  for (size_t at = 0; at < Slots_.size(); ++at) {
    const Slot &owned = Slots_[at];
    if (!owned.Held) { continue; }
    for (size_t pair = 0; pair < owned.PairCount; ++pair) {
      if (owned.Pairs[pair].To == of && RuleOf(owned.Pairs[pair].How).OwnedByTarget) {
        Remove(Entity{(uint32_t)at, owned.Generation});
        break;
      }
    }
  }
}

bool Store::Alive(Entity of) const { return Held(of) != nullptr; }

Role Store::RoleOf(Entity of) const {
  const Slot *slot = Held(of);
  return slot == nullptr ? Role::Body : slot->Is;
}

bool Store::Give(Entity to, Tag tag) {
  Slot *slot = const_cast<Slot *>(Held(to));
  if (slot == nullptr) { return Refuse("a tag cannot be given to what does not stand"); }
  if (tag.Value == 0) { return Refuse("the empty tag is not in the catalogue"); }
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

bool Store::Offer(Entity at, Tag activity, size_t seats) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (slot == nullptr) { return Refuse("an offer needs its object standing"); }
  if (activity.Value == 0) { return Refuse("an offer advertises an activity from the catalogue"); }
  if (seats == 0 || seats > kSeatsPerOffer) {
    return Refuse("an offer holds between one seat and the pool's few");
  }
  if (slot->Offers.Value != 0) { return Refuse("this object already advertises, and one object is one offer"); }
  slot->Offers = activity;
  slot->SeatCount = seats;
  for (size_t seat = 0; seat < seats; ++seat) { slot->Seats[seat] = Taken{}; }
  return true;
}

size_t Store::Offering(Tag activity, Entity into[], size_t room) const {
  size_t found = 0;
  for (size_t at = 0; at < Slots_.size(); ++at) {
    const Slot &slot = Slots_[at];
    if (!slot.Held || slot.Offers.Value == 0 || !slot.Offers.Within(activity)) { continue; }
    if (into != nullptr && found < room) { into[found] = Entity{(uint32_t)at, slot.Generation}; }
    ++found;
  }
  return found;
}

bool Store::Claim(Entity by, Entity at) {
  Slot *slot = const_cast<Slot *>(Held(at));
  if (Held(by) == nullptr || slot == nullptr) { return Refuse("a claim needs both of its ends standing"); }
  if (slot->Offers.Value == 0) { return Refuse("this object advertises nothing to claim"); }
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
  if (rule.Exclusive && !(TargetOf(from, how) == kNoEntity)) {
    return Refuse(std::string(Named(how)) + " is exclusive, and this source already has its target");
  }
  if (rule.SourceDoes.Value != 0 && !Has(from, rule.SourceDoes)) {
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
  size_t kept = 0;
  for (size_t at = 0; at < writable->PairCount; ++at) {
    if (Held(writable->Pairs[at].To) != nullptr) { writable->Pairs[kept++] = writable->Pairs[at]; }
  }
  writable->PairCount = kept;
  if (writable->PairCount == kPairsPerEntity) {
    return Refuse("this entity carries all the connections it can");
  }
  writable->Pairs[writable->PairCount++] = Pair{how, to};
  return true;
}

Entity Store::TargetOf(Entity of, Relation how) const {
  const Slot *slot = Held(of);
  if (slot == nullptr) { return kNoEntity; }
  for (size_t at = 0; at < slot->PairCount; ++at) {
    if (slot->Pairs[at].How == how && Held(slot->Pairs[at].To) != nullptr) {
      return slot->Pairs[at].To;
    }
  }
  return kNoEntity;
}

size_t Store::Targets(Entity of, Relation how, Entity into[], size_t room) const {
  const Slot *slot = Held(of);
  if (slot == nullptr) { return 0; }
  size_t found = 0;
  for (size_t at = 0; at < slot->PairCount; ++at) {
    if (slot->Pairs[at].How != how || Held(slot->Pairs[at].To) == nullptr) { continue; }
    if (into != nullptr && found < room) { into[found] = slot->Pairs[at].To; }
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
  if (!Alive(instance) || !Link(instance, Relation::IsA, prefab)) { return kNoEntity; }
  for (size_t at = 0; at < Slots_.size(); ++at) {
    const Slot &child = Slots_[at];
    if (!child.Held) { continue; }
    const Entity childId{(uint32_t)at, child.Generation};
    if (childId == instance) { continue; }
    if (!(TargetOf(childId, Relation::ChildOf) == prefab)) { continue; }
    const Entity copied = Instantiate(childId);
    if (!Alive(copied) || !Link(copied, Relation::ChildOf, instance)) { return kNoEntity; }
  }
  return instance;
}

Entity Store::CopyOf(Entity instance, Entity prefabChild) const {
  if (Held(instance) == nullptr) { return kNoEntity; }
  for (size_t at = 0; at < Slots_.size(); ++at) {
    const Slot &child = Slots_[at];
    if (!child.Held) { continue; }
    const Entity childId{(uint32_t)at, child.Generation};
    if (!(TargetOf(childId, Relation::ChildOf) == instance)) { continue; }
    if (TargetOf(childId, Relation::IsA) == prefabChild) { return childId; }
  }
  return kNoEntity;
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
