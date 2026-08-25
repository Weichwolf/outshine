"""The glTF-Validator's own corpus: deliberately malformed assets and Khronos's golden reports."""

import re

from prep import fetching
from prep.refusal import Refusal

VALIDATOR_HOST = "raw.githubusercontent.com"
VALIDATOR_REPO = "KhronosGroup/glTF-Validator"

VALIDATOR_URL = re.compile(
    r"^https://" + re.escape(VALIDATOR_HOST) + "/" + re.escape(VALIDATOR_REPO)
    + r"/([0-9a-f]{40})/(.+)$"
)

def check_url(url, commit=None):
    match = VALIDATOR_URL.match(url)
    if not match:
        raise Refusal(
            "url " + url,
            expected="the pinned glTF-Validator tree",
            observed=url,
            why="an unlisted source has no pin, no stated licence and no reason to be trusted",
        )
    if commit is not None and match.group(1) != commit:
        raise Refusal(
            "url " + url,
            expected="the manifest's pinned commit " + commit,
            observed="a url naming commit " + match.group(1),
            why="a url that carries its own pin must carry the declared one",
        )
    return "gltf-validator"

_OPENER = fetching.opener(check_url)

def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    check_url(url, commit)
    if member is not None:
        raise Refusal("fetch " + url, expected="a plain file",
                      observed="an archive member", why="this corpus ships no archives")
    return fetching.to_store(_OPENER, url, expected_sha256, store, expected_bytes)
