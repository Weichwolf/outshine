"""Change only enumeration in a private copy of the actual direct shader (#448)."""

import hashlib
from pathlib import Path


def prepare_shaders(repository, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    source = Path(repository) / "src/render/shaders"
    material = (source / "material.glsl").read_text()
    marker = "layout(std140, set = 3, binding = 0)"
    if material.count(marker) != 1:
        raise ValueError("material declaration changed; re-audit the private control")
    (output / "localLightMaterial.glsl").write_text(material.split(marker)[0])
    text = (source / "subjectLighting.glsl").read_text()
    original_hash = hashlib.sha256(text.encode()).hexdigest()
    changes = {
        "  int count = int(lights.count.x);": """#if LOCAL_LIGHT_LISTED
  uvec2 nativeSpan = lightSpan(p);
  int count = int(words[1] + nativeSpan.y);
#else
  int count = int(lights.count.x);
#endif
#if LOCAL_LIGHT_FIRST16
  count = min(count, 16);
#endif""",
        "    Light light = lights.items[at];": """#if LOCAL_LIGHT_LISTED
    uint lightIndex = lightIndexAt(nativeSpan, at);
#else
    uint lightIndex = uint(at);
#endif
    Light light = emitters[lightIndex];""",
        "int(lights.count.z) == at + 1": """int(lights.count.z) ==
#if LOCAL_LIGHT_WRONG_OWNER
        at + 1
#else
        int(lightIndex) + 1
#endif
""",
        "    if (attenuation <= 0.0) { continue; }": "    if (attenuation <= 0.0) { continue; }\n    supportCount += 1;",
    }
    for old, new in changes.items():
        if text.count(old) != 1:
            raise ValueError(
                "direct shader changed; re-audit enumeration/instrumentation"
            )
        text = text.replace(old, new)
    (output / "localLightSubject.glsl").write_text(text)
    return {
        "actual_direct_shader_sha256": original_hash,
        "private_direct_shader_sha256": hashlib.sha256(text.encode()).hexdigest(),
        "changes": changes,
        "scope": "Identical existing BRDF, point/spot attenuation and atlas visibility body. Private emitter enumeration and support counter only. Environment is a prescribed black control; no texture, Coverage or transparent compositing acceptance.",
    }
