"""Executable tapered conifer wood using existing graph placement and profiles."""

import numpy as np
from generator_specs import Generator, Placement, TubeShape
from outshine_ref.vegetation import DistancePattern, Profile
from parent_profile_graph import ParentProfileGraph


def affine_profile(first, last, start, end):
    if not first < last:
        raise ValueError("nonempty physical parent interval required")
    x = np.linspace(0, 1, 4)
    return Profile(start + (end - start) * (x - first) / (last - first))


def placement(level, parent_length):
    fixed = level.branches == 1
    groups = (level.branches - 1) // level.whorl_size + 1
    if not fixed and (groups < 2 or level.branches % level.whorl_size):
        raise ValueError("complete multiple physical whorls required")
    pattern = (
        None
        if fixed
        else DistancePattern(
            spacing=Profile(
                np.full(4, (level.last - level.first) * parent_length / (groups - 1))
            ),
            whorl_size=level.whorl_size,
            limit_groups=groups,
        )
    )
    scale = (
        Profile(np.ones(4))
        if fixed
        else affine_profile(level.first, level.last, 1, level.tip_scale)
    )
    return Placement(
        count=1,
        first=level.first,
        last=level.last,
        pitch=level.angle,
        pitch_variance=level.angle_variation,
        azimuth_variance=level.azimuth_variation,
        scale=scale,
        scale_variance=0,
        pattern=pattern,
    )


def mature_graph(plant):
    levels = plant.wood.levels
    if len(levels) != 4 or any(level.length_shape != "taper" for level in levels[1:]):
        raise ValueError("four-order tapered conifer input required")
    if levels[1].branches % levels[1].whorl_size:
        raise ValueError("complete primary branch whorls required")
    nodes = [Generator(0, "root", "root", ())]
    inclinations, biases = [], []
    for order, level in enumerate(levels):
        key = order + 1
        source = TubeShape(
            length=level.length,
            bend=level.bend,
            radius=Profile(np.linspace(level.base_radius, level.tip_radius, 4)),
        )
        settings = (
            Placement(
                first=0,
                last=0,
                pitch=0,
                pitch_variance=0,
                azimuth_variance=0,
                scale_variance=0,
            )
            if order == 0
            else placement(level, levels[order - 1].length)
        )
        nodes.append(
            Generator(
                key,
                ("stem", "primary", "secondary", "terminal")[order],
                "axis",
                (key - 1,),
                source,
                settings,
            )
        )
        if level.angle_tip is not None and order:
            inclinations.append(
                (
                    key,
                    affine_profile(
                        level.first, level.last, level.angle, level.angle_tip
                    ),
                )
            )
        if level.upward_bias:
            biases.append((key, level.upward_bias))
    return ParentProfileGraph(
        plant.wood.seed,
        tuple(nodes),
        parent_length_ratios=tuple(
            (order + 1, levels[order].length / levels[order - 1].length)
            for order in (2, 3)
        ),
        inclination_profiles=tuple(inclinations),
        upward_biases=tuple(biases),
    )
