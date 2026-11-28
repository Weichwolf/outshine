import unittest

import numpy as np
from outshine.rendering.image import gl_context


class GlslShadowTest(unittest.TestCase):
    def test_shadow_bias_uses_the_same_incidence_on_both_sides(self):
        try:
            import moderngl
            from outshine.rendering import glsl
        except ImportError as error:
            self.skipTest(f"GLSL reference dependencies unavailable: {error}")
        try:
            context = gl_context()
        except Exception as error:  # noqa: BLE001 - glcontext can raise a plain Exception.
            self.skipTest(f"GLSL context unavailable: {error}")

        with context as ctx:
            source = glsl.shader(glsl.HERE / "reference.frag")
            declaration = source[: source.rindex("void main(){")]
            fragment = (
                declaration
                + """
void main() {
    vec3 normal = vec3(0., 0., gl_FragCoord.x < 1. ? 1. : -1.);
    colour = vec4(sunVisibility(vec3(0.), normal), 0., 0., 1.);
}
"""
            )
            program = ctx.program(vertex_shader=glsl.SCREEN, fragment_shader=fragment)
            projection = np.eye(4, dtype="f4").tobytes()
            for name in ("shadowMatrix", "localShadowMatrix", "nearShadowMatrix"):
                program[name].write(projection)
            program["sun"].value = (0, 0, 1)
            program["regionalShadowSpan"].value = glsl.REGIONAL_SHADOW_SPAN
            program["localShadowSpan"].value = glsl.LOCAL_SHADOW_SPAN
            program["nearShadowWidth"].value = glsl.NEAR_SHADOW_WIDTH

            size = glsl.NEAR_SHADOW_RESOLUTION
            distant = ctx.depth_texture((1, 1))
            distant_fb = ctx.framebuffer(depth_attachment=distant)
            distant_fb.use()
            distant_fb.clear(depth=1)
            near = ctx.depth_texture((size, size))
            near.filter = (moderngl.LINEAR, moderngl.LINEAR)
            near_fb = ctx.framebuffer(depth_attachment=near)
            for unit, (name, texture) in enumerate(
                (
                    ("shadowMap", distant),
                    ("localShadowMap", distant),
                    ("nearShadowMap", near),
                )
            ):
                texture.use(unit)
                program[name].value = unit

            view = ctx.simple_framebuffer((2, 1), components=4, dtype="f4")
            quad = ctx.vertex_array(program, [])
            ctx.disable(moderngl.DEPTH_TEST)
            for gap, expected in ((0.0, 1.0), (0.04, 0.0)):
                with self.subTest(gap=gap):
                    near_fb.use()
                    near_fb.clear(depth=0.5 - gap / glsl.LOCAL_SHADOW_SPAN[1])
                    view.use()
                    quad.render(vertices=3)
                    visibility = np.frombuffer(
                        view.read(components=1, dtype="f4"), dtype="f4"
                    )
                    np.testing.assert_array_equal(visibility, [expected, expected])
