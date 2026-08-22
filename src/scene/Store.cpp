#include "Store.h"

namespace outshine {

namespace {

const char *Named(Kind kind) {
  switch (kind) {
    case Kind::Body: return "a body";
    case Kind::Mind: return "a mind";
    case Kind::Tool: return "a tool";
    case Kind::Assignment: return "an assignment";
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

Entity Store::Add(Kind kind) {
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
}

bool Store::Alive(Entity of) const { return Held(of) != nullptr; }

Kind Store::KindOf(Entity of) const {
  const Slot *slot = Held(of);
  return slot == nullptr ? Kind::Body : slot->Is;
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

bool Store::Link(Entity from, Relation how, Entity to) {
  const Slot *source = Held(from);
  const Slot *target = Held(to);
  const Rule &rule = RuleOf(how);
  if (source == nullptr || target == nullptr) {
    return Refuse(std::string(Named(how)) + " needs both of its ends standing");
  }
  if ((rule.TargetKinds & KindBit(target->Is)) == 0) {
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
