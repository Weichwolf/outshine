"""Source identity for reproducible local experiment reports."""

import hashlib
from pathlib import Path
import subprocess

INVENTORY = Path(__file__).resolve().parents[3]/'doc/references/data/osm/openfreemap-inventory-20261009.json'


def revision():
    directory = Path(__file__).resolve().parent
    root = directory.parents[2]
    paths = sorted(directory.glob('*.py'))+[directory/'requirements.txt', INVENTORY]
    return dict(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                experiment_tree_dirty=bool(subprocess.check_output(['git', 'status', '--porcelain', '--',
                                                                    'test/experiments/buildings'], cwd=root, text=True)),
                implementation_sha256={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                                       for p in paths})
