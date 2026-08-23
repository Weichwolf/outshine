Type: bug
Area: clients

**The SDL_image door bounds its output the way the PNG door already does**

src/clients/Image.cpp:35-51: `DecodeImage` hands hostile bytes to `IMG_Load_IO` and then
allocates `Width * Height * 4` with no bound on either side. The tree's OTHER decode door
already names the bound with its origin — src/core/io/Png.cpp:11-14, `kMaxSide = 16384`,
"[SET] the device's own max texture dimension" — and refuses with both numbers. A glTF whose
embedded texture decodes to 65535×65535 buys a 17 GiB resize on an 8 GB device: OOM kill
instead of a refusal by name. DecodeImage feeds Surfaces.cpp:34/154 straight from content
files; this is the engine's content boundary and must be defensive.

Demanded: the same `kMaxSide` bound (one origin, shared or restated with its derivation)
checked after decode and before the resize, refusal carrying both numbers; a unit twin in
test/unit/clients that hands a decodable image over the bound and reads the refusal.

---

Closed -- DecodeImage bounds both sides at the device's own 16384 (the PNG door's bound,
restated with its origin) BEFORE the convert and the engine's own resize follow. Proven in
unit/clients/ADecodedImageIsBoundedLikeItsSiblingDoor: an 8x8 BMP decodes as itself, a
17000x1 and a 1x17000 refuse (negative control red). Honest residue: SDL_image's own
internal allocation for a declared-huge but truncated file happens before this bound can
see the dims -- the engine's allocations are what this door can and now does bound.
