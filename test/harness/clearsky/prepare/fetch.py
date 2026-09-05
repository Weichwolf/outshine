"""Bruneton's clear-sky-models inputs at one commit: Kider et al.'s measured day and ASTM G173, pinned by digest."""
import re

from prep import fetching
from prep.refusal import Refusal

SKY_URL = re.compile(
    r"^https://raw\.githubusercontent\.com/ebruneton/clear-sky-models/[0-9a-f]{40}/input/"
    r"[A-Za-z0-9._%-]+$"
)


def check_url(url, commit=None):
    if commit is None:
        raise Refusal("url " + url, expected="a commit pin",
                      observed="none",
                      why="a mirror moves; the commit is what makes the bytes the measurement's")
    if not SKY_URL.match(url) or ("/" + commit + "/") not in url:
        raise Refusal("url " + url, expected="the clear-sky-models repository's input directory at the pinned commit",
                      observed=url,
                      why="an unlisted source has no pin, no stated licence and no reason to be trusted")
    return "ebruneton"


_OPENER = fetching.opener(check_url)


def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    check_url(url, commit)
    if member is not None:
        raise Refusal("fetch " + url, expected="a plain file", observed="an archive member",
                      why="this corpus ships plain files")
    return fetching.to_store(_OPENER, url, expected_sha256, store, expected_bytes)


def unpack(kind, source, destination):
    raise Refusal("unpack " + str(kind), expected="nothing to unpack", observed=str(kind),
                  why="this corpus ships plain files")
