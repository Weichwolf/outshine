Type: issue
Area: clients
Tags: boundary

# SDL enters through one optional header

Owner ruling (2026-08-22): the core door stays SDL-free -- the engine owns SDL init and the
default client never touches it (declare, Run; SDL_InitSubSystem is refcounted so a host that
initialised SDL coexists). The GUEST path (client owns window and loop) gets exactly one
opt-in adapter header, include/outshine/HostSdl.h, the only public header that spells an SDL
type: window/surface in, frame out, wrapping the PresentInto seam. Everything the headless
build proves today stays true: the driver suite links -lz -lcurl only, so the library minus
the render column is SDL-independent by construction. Lands with the 1582 boundary
enforcement; the viewer becomes the first HostSdl client and stops including src/.
