"""Retain encoded material images and sampler links independently of source geometry buffers."""

from dataclasses import dataclass


@dataclass(frozen=True)
class ImageResource:
    name: str
    mime_type: str
    encoded: bytes


@dataclass(frozen=True)
class Resources:
    images: tuple = ()
    textures: tuple = ()
    samplers: tuple = ()
