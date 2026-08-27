Type: issue
State: active
Parent: 1953
Area: engine

# `Live` is four things and should hold one of them

`src/engine/Live.{h,cpp}` -- 770 lines of body, 48 methods over 38 members -- carries four
concerns that share nothing but the object they were written into:

| concern | state it holds |
|---|---|
| a POSED ASSET | `File_` `Geometry_` `Motion_` `Variant_` `Locals_` `Weights_` `Frames_` `At_` `Moves_` `PreviousPositionsM_` `FileStands_` |
| the SUBMISSION to the renderer | `Stood_` `Looking_` `Scratch_` `Table_` `SentBody_` `SentBuilt_` `PartBounds_` `Joined_` `Carrying_` `Stoodup_` |
| the SKY and its shadow | `SkyToSun_` `SkyUp_` `SkyStands_` `ShadowRadiusStoodM_` |
| the OVERLAY on the screen | `Laid_` `Scrolled_` `Quads_` `Font_` `Around_` |

A glTF document reader and a scrollable screen overlay in one class is not an implementation, it
is an object that everything was added to because it was already there. The counting bears it out:
31 lines of the body touch overlay state and 18 touch the asset, and no line touches both.

**The benchmark separates all four and neither engine blurs them.** Unreal has `UWorld` for the
scene, `FSceneView` for the camera, `FPrimitiveSceneProxy` for what draws, `UMG` for the overlay
and an importer for the asset -- five objects, five lifetimes. RAGE separates the map entity, the
draw list, Scaleform and the resource. What they agree on is the RULE: an object's members should
share a lifetime and a reason to change, and these four share neither.

- [x] the overlay is its own type: `Core::Overlay` in `src/engine/Overlay.{h,cpp}` holds the laid
      documents, the scroll state, the quads and the face, and `Shows` -- the DECLARATION of a
      surface -- went with it. `Live` keeps three one-line delegations and none of the state.
      `Wheeled` returns whether the scroll moved instead of recomposing itself, so the scroll
      state and the composition are two steps rather than a call back into the owner
- [x] the posed asset is its own type: `Core::Asset` in `src/engine/Asset.{h,cpp}` holds the
      document, the built subject, the animation, the variant selection, the previous pose and
      the cursor over it, and answers `Reads`, `Poses`, `Clears`, `Advances`. Eleven members
      leave `Live`
- [x] the sky's three fields reach the renderer through its own door: `Renderer::SetSkyEye` and
      `SkyStage::Eye`/`Stands`, so nothing is mirrored engine-side to be replayed
- [ ] what remains of `Live` coordinates those and holds the submission
- [ ] proof: khronos/glTF/WaterBottle and khronos/glTF/BoxAnimated stay green through every move,
      and `--audit-layers` stays green
