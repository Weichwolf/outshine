Type: task
Parent: 1480
Area: clients
Tags: scope

**A player is a body input drives and a view looks through**

**The engine ships one, and the reason is that no two games would spell it alike.** Input, a body and a
camera meet in exactly one place in every game there is, and leaving that meeting to the client means
every client re-invents walking.

```xml
<player is="settler" starts="sanctuary" view="eyes"
        eyeHeightM="1.75" walkMs="1.4" runMs="4.5"/>

<view id="eyes"             follows="player" person="first" offsetY="1.7" fovDeg="80"/>
<view id="over-the-shoulder" follows="player" person="third" offsetY="1.6"
      distanceM="2.5" pitchLimitDeg="70" fovDeg="70"/>
<view id="aimed"            follows="player" person="first" offsetY="1.7" fovDeg="45"
      timeScale="0.2"/>
```

**A player IS an instance of a kind** -- so it has attributes, holds things, and can be looked at by
another actor's `look` reflex. What makes it the player is that **the declared input actions drive it**
and a view follows it. *Nothing else about it is special, which is what lets a scenario make the mayor
playable by changing one line.*

## First and third person are one mechanism with two settings

| | first | third |
|---|---|---|
| where the eye is | at `eyeHeightM` on the body | `distanceM` back along the look direction |
| what is drawn | the body is not, or its arms only | the whole body |
| the pitch limit | 89 deg, so the neck does not invert | less, because the body gets in the way |
| **what is shared** | the ray from the eye, the collision that keeps it out of walls, the follow | |

**The engine spells the two and no third.** A fixed camera, an isometric one and a map view are all
`third` with a different distance and a locked orientation, which is a scenario's business.

## What must be true

- [x] **A scenario declares a player and 0 or 1..N views**, and which view is live
- [ ] **Declared input actions drive the player's body** and nothing else, so a scenario with no player
  is a scenario nobody walks around in -- a studio shot, which is most of the render corpus
- [ ] **A third-person camera does not enter geometry**: it is a swept query back along the look
  direction, which is the visibility structure `board:1464` already refits
- [ ] **The eye is the audio listener** (`board:1486`), and it is the LIVE view's rather than the
  player's, so a map view hears from the map
- [ ] **Walking is the one physics system**, not a camera that flies: `walkMs` and `runMs` are speeds
  the body is given, and the ground it stands on is Ground's
- [ ] **Input to photon is measured for the player specifically**, because it is the number a player
  feels and `CLAUDE.md` names it as the feature nobody declares
