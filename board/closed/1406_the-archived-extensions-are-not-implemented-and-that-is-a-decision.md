Type: issue
Area: gltf
Tags: scope, khronos

**The archived extensions are not implemented, and that is a decision rather than a gap**

[OWNER] *was heisst archivierte extension? wenn sie obsolet ist, nicht implementieren.*

**What "archived" means at Khronos**: ratified, and no longer recommended. `KHR_materials_pbrSpecularGlossiness`
is archived because the core's own metallic-roughness model replaced it; the extension survives only
because older exporters wrote it. `KHR_techniques_webgl` and `KHR_xmp` are archived for their own
reasons and are covered by the same decision.

**So they are not built.** The engine speaks one material model -- *the core dictates the pipeline* --
and a second parameterisation would either be a second model inside the engine or a conversion at the
reader. The first is what this decomposition exists to prevent; the second would be a conversion of
TEXTURES at load, not merely of numbers, and it would exist to serve a format the format itself has
moved on from.

## What it costs, stated so nobody has to rediscover it

[MEASURED] at the pin, exactly **one** model requires an archived extension: `SpecGlossVsMetalRough`,
whose two materials BOTH carry `diffuseTexture` and one a `specularGlossinessTexture` -- so even a
generous reading would need the texture conversion rather than the arithmetic one.

**Its case therefore stays red, with a named cause that is not a defect.** *The reader refuses the file
by name, which is the loud refusal the format asks for, and nothing is drawn wrongly.*

## What this decides for the rest

**`board:1382`'s population is the 20 RATIFIED extensions and nothing else**, which is what it already
said and now has a reason on the board rather than only in its own prose. An extension moving from
in-development to ratified is how that population grows; an archived one never enters it.
