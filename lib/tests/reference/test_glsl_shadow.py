import unittest
from math import pi, tan

import numpy as np
from outshine.rendering.image import gl_context


class GlslShadowTest(unittest.TestCase):
    def test_shadow_caster_culling_preserves_clipped_depth(self):
        try:
            import moderngl
            from outshine.rendering import glsl

            context = gl_context()
        except Exception as error:  # noqa: BLE001 - glcontext can raise a plain Exception.
            self.skipTest(f"GLSL context unavailable: {error}")

        with context as ctx:
            program = ctx.program(
                vertex_shader=glsl.shader(glsl.HERE / "reference.vert"),
                fragment_shader="#version 410 core\nvoid main() {}\n",
            )
            ctx.enable(moderngl.DEPTH_TEST)
            placements = [
                (0, 0, 0, 0, 0.3),
                (1.5, 0.5, -0.3, pi, 0.6),
                (0, -1.4, 0.2, pi / 2, 0.5),
                (4, 0, 0, 0, 1),
                (0, 0, 3, 0, 1),
            ]
            batch = glsl.upload(
                ctx,
                program,
                "foliage",
                [(0, 0, 0), (2, 0, 0), (0, 0.8, 0)],
                [(0, 1, 2)],
                placements,
            )
            projection = np.eye(4, dtype="f4")
            light = projection.T.tobytes()
            casters, resources = glsl.shadow_casters(ctx, program, [batch], projection)
            self.assertEqual([count for count, _ in casters], [3])
            full = glsl.shadow_depth(
                ctx, program, [(len(placements), batch.vao)], light, 64
            )
            culled = glsl.shadow_depth(ctx, program, casters, light, 64)
            before = full.read()
            self.assertLess(np.frombuffer(before, dtype="f4").min(), 1)
            self.assertEqual(culled.read(), before)
            for resource in (full, culled, *resources):
                resource.release()
            batch.release()

    def test_local_solar_shadows_keep_contact_and_soften_with_distance(self):
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
uniform bool shadowStrip;
void main() {
    if (shadowStrip) {
        float x = (gl_FragCoord.x / 512. - .5) * .4;
        colour = vec4(sunVisibility(vec3(x, 0., 0.), vec3(0., 0., 1.)), 0., 0., 1.);
        return;
    }
    vec3 normal = vec3(0., 0., gl_FragCoord.x < 1. ? 1. : -1.);
    colour = vec4(sunVisibility(vec3(0.), normal), 0., 0., 1.);
}
"""
            )
            program = ctx.program(vertex_shader=glsl.SCREEN, fragment_shader=fragment)
            scale = 2 / glsl.NEAR_SHADOW_WIDTH
            projection = np.diag([scale, scale, 1, 1]).astype("f4").tobytes()
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
            raw_depth = [
                ctx.sampler(
                    texture=texture,
                    repeat_x=False,
                    repeat_y=False,
                    filter=(moderngl.NEAREST, moderngl.NEAREST),
                )
                for texture in (distant, near)
            ]
            for unit, (name, sampler) in enumerate(
                zip(("localDepthMap", "nearDepthMap"), raw_depth, strict=True), 3
            ):
                sampler.use(unit)
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

            program["shadowStrip"].value = True
            strip = ctx.simple_framebuffer((512, 1), components=4, dtype="f4")
            x = ((np.arange(512) + 0.5) / 512 - 0.5) * 0.4
            for gap, tolerance in ((2.0, 0.003), (8.0, 0.006), (32.0, 0.025)):
                with self.subTest(blocker_distance=gap):
                    near_fb.use()
                    near_fb.clear(depth=1)
                    near_fb.clear(
                        depth=0.5 - gap / glsl.LOCAL_SHADOW_SPAN[1],
                        viewport=(0, 0, size // 2, size),
                    )
                    strip.use()
                    quad.render(vertices=3)
                    visibility = np.frombuffer(
                        strip.read(components=1, dtype="f4"), dtype="f4"
                    )
                    # Visible fraction of the uniform solar disc past a straight edge.
                    t = np.clip(x / (gap * tan(0.0046542)), -1, 1)
                    expected = 0.5 + (np.arcsin(t) + t * np.sqrt(1 - t * t)) / np.pi
                    self.assertLess(np.mean(np.abs(visibility - expected)), tolerance)
