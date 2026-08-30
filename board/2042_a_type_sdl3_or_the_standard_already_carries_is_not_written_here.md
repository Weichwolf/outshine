Type: task
State: open
Area: src
Tags: hygiene, sdl3, cpp23

# A type SDL3 or the standard already carries is not written here

**Benchmark** — Unreal: writes its OWN containers (`TArray`, `TArrayView`) and its own RHI enums, because it predates the standard library it would have used and ships on platforms that had none. RAGE: the same, for the same reason. **Taking NEITHER**, and the reason is that this is not a structural question they answer -- both chose in the 2000s against a C++03 standard library and a vendor-specific graphics layer, and outshine has C++23 and one hard dependency on SDL3. This tree's own rule already decides it: **where SDL3 supplies the structure or the function, it is the one used**, and `std::span` is named in CLAUDE.md by name.

## What is duplicated, measured

    duplicate                        of                              uses   files
    outshine::Span<T>                std::span<T>                     136      56
    SubjectWrap / SubjectFilter /    SDL_GPUSamplerAddressMode /       --       2
      SubjectMip                       SDL_GPUFilter /
                                       SDL_GPUSamplerMipmapMode
    import::Wrap / Filter /          outshine::Wrap / Filter /         --       2
      MipFilter                        MipFilter (include/Texture.h)

**`outshine::Span` is the large one and the clearest.** `src/base/spatial/Span.h` is 41 lines
reimplementing `std::span` with `Data()`, `Size()`, `Empty()`, `Bytes()` and `Sub()` -- the same
four plus `subspan`, spelled differently. It costs a reader who knows the standard nothing to
learn and everything to remember, and it cannot be handed to any standard algorithm that takes a
range without being unwrapped first. board:1621 asks for `std::span` at every boundary; this is
the same sweep seen from the other end, so **it belongs to board:1621** and is recorded there
rather than worked twice.

**The sampler trio is the SDL3 one.** `SubjectWrap`, `SubjectFilter` and `SubjectMip` live in
`src/render/stages/SubjectTypes.h` -- the render tier, which links SDL3 -- and
`SubjectResidency.cpp` carries `AddressOf` and `FilterOf` to convert them into
`SDL_GPUSamplerAddressMode` and `SDL_GPUFilter`, arm for arm, with the same three and two values.
That is a table kept true by hand to a driver nobody here wrote, which is exactly the shape this
tree's SDL3 rule refuses. The render tier's fields become the SDL types and the two functions go.

**`include/Texture.h`'s trio is NOT a duplicate and stays.** The DOOR has to speak the client's
word rather than the device's, and `outshine::Wrap::Repeat` is a name a Filament reader owns where
`SDL_GPU_SAMPLERADDRESSMODE_REPEAT` is one they do not. The translation belongs at the boundary and
happens ONCE -- which is the arrangement CLAUDE.md already states. What must go is the THIRD
spelling in the middle: `src/import/Types.h` repeats the door's own enum for the importer's use,
so the importer resolves a glTF number into an import enum, which a surface pass turns into a
render enum, which the residency turns into SDL's. Four spellings of three values.

## What will be true

- [ ] `src/base/spatial/Span.h` is deleted and `std::span` stands in its place -- tracked on
      board:1621, which owns the sweep
- [ ] `SubjectTexture` holds `SDL_GPUSamplerAddressMode`, `SDL_GPUFilter` and
      `SDL_GPUSamplerMipmapMode`, and `AddressOf`/`FilterOf` are gone
- [ ] `src/import/Types.h` names `outshine::Wrap`, `outshine::Filter` and `outshine::MipFilter`
      rather than repeating them, so a value is spelled TWICE across the tree -- once at the door
      and once at the device -- and never three or four times

## What this does NOT cover

**The RAII handles stay.** `OwnedBuffer`, `OwnedTexture`, `OwnedSampler`, `OwnedComputePipeline`
are ownership over an SDL handle, which is the one thing SDL's C interface cannot express and
CLAUDE.md already exempts by name. They are not duplicates of anything.

**`std::thread` and friends stay.** SDL3 has `SDL_Thread` and `SDL_Mutex`, and the rule says to
prefer SDL where it supplies the structure -- but a C++23 `std::jthread` with `std::stop_token`
supplies MORE structure than SDL's handle does, and the tier that uses them (`src/world`,
`src/host`) is not the device layer. This is named here so the question is not reopened by
somebody reading the rule alone.

**No measurement is claimed.** Nothing in this item makes a frame faster; it removes spellings.
The number it moves is how many words a reader has to hold, and there is no instrument for that.
