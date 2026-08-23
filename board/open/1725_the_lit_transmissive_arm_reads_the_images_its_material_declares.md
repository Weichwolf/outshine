Type: bug
Area: render
Tags: instrument

**The lit transmissive arm reads the images its material declares**

Found while closing 1145: SubjectDraw::FragmentEntry returns "fsLitTransmissive" for a
transmissive surface on a tangentless lit layout REGARDLESS of textured -- the colour tap,
the metal-rough tap and the emissive tap are all dropped for a lit, textured, transmissive
part without a normal map (glass with a albedo/roughness image and no normal map shades
from factors alone). The same class as 1145, one arm over: the tap belongs to
textured-and-lit, and the transmissive shader family needs its textured variant (or the
selection collapses textured into the arm the way the opaque family does). The proof shape
exists: AMetalRoughImageReadsWithoutATangentFrame renders a quad twice and demands the
frames differ -- a transmissive twin of that fixture discriminates.
