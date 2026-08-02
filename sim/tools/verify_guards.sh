#!/bin/bash
# verify-guards: the tree's compile-time guarantees, PROVEN by trying to break them.
#
# Every guarantee here is a private member with exactly one friend, or a return value that may not be
# dropped. Such a rule is only worth its comment if somebody has checked that violating it FAILS TO
# COMPILE — so each case below is a two-line translation unit that must be REJECTED, plus one that must
# be ACCEPTED. The accepted one is not decoration: without it a broken include path would "reject"
# everything and the gate would pass while proving nothing.
#
# Usage: tools/verify_guards.sh "<include flags>"   (the Makefile passes core-lib's own)
set -u
cd "$(dirname "$0")/.."
INC="${1:-}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
CXX_FLAGS="-std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter $INC"

pass=0
fail=0

# $1 = name, $2 = "reject"|"accept", $3 = body
guard() {
  local name="$1" want="$2" body="$3" f="$TMP/$1.cpp" out
  printf '%s\n' "$body" > "$f"
  out=$(c++ $CXX_FLAGS "$f" 2>&1)
  local rc=$?
  if [ "$want" = reject ] && [ $rc -eq 0 ]; then
    echo "verify-guards: $name COMPILED — the guarantee it tests is gone" >&2
    fail=$((fail + 1))
    return
  fi
  if [ "$want" = accept ] && [ $rc -ne 0 ]; then
    echo "verify-guards: $name did NOT compile, and it must — the gate cannot judge the rest:" >&2
    echo "$out" | head -5 >&2
    fail=$((fail + 1))
    return
  fi
  pass=$((pass + 1))
}

# ---- 1. A client cannot advance a simulation without being told whether the run is over.
guard advance_discarded reject '
#include "FBMissionSim.h"
void Frame(FlightBox::Missions::FBMissionSim &sim) { sim.Advance(0.1); }
'
guard advance_asked accept '
#include "FBMissionSim.h"
bool Frame(FlightBox::Missions::FBMissionSim &sim) {
  return sim.Advance(0.1) == FlightBox::Missions::FBRunState::Concluded;
}
'

# ---- 2. A client cannot write its own tick: the unit surface a loop needs is friend-locked.
guard unit_stepped reject '
#include "FBSimUnit.h"
void Loop(FlightBox::Units::FBSimUnit &u) { u.Run(0.1, nullptr, nullptr); }
'
guard barrier_published reject '
#include "FBSimUnit.h"
void Loop(FlightBox::Units::FBSimUnit &u) { u.PublishPose(); }
'
guard judges_skipped reject '
#include "FBSimUnit.h"
void Loop(FlightBox::Units::FBSimUnit &u) { u.UpdateGroundAsl(0.0); }
'

# ---- 3. Health is monotone and has ONE writer: no self-repair, and no forged K.O. either.
guard self_repair reject '
#include "FBSystemHealth.h"
void Heal(FlightBox::FBSystemHealth &h) {
  h.Worsen(FlightBox::FBSystemId::Engine, FlightBox::FBHealthState::Intact);
}
'
guard ko_forged reject '
#include "FBSystemHealth.h"
void Kill(FlightBox::FBSystemHealth &h) { h.NoteDestroyed(); }
'
guard ko_read accept '
#include "FBSystemHealth.h"
bool Alive(const FlightBox::FBSystemHealth &h) { return !h.Destroyed(); }
'

if [ $fail -ne 0 ]; then
  echo "verify-guards: FAILED ($fail of $((pass + fail)) guards)" >&2
  exit 1
fi
echo "verify-guards: $pass/$pass guards hold (6 rejected as they must, 2 accepted)"
