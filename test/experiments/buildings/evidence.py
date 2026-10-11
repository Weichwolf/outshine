"""Source identity for reproducible local experiment reports."""

import hashlib
from pathlib import Path
import subprocess

from samples import INVENTORY


def revision():
    directory = Path(__file__).resolve().parent
    root = directory.parents[2]
    paths = sorted(directory.glob('*.py'))+[directory/'requirements.txt', INVENTORY]
    return dict(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                experiment_tree_dirty=bool(subprocess.check_output(['git', 'status', '--porcelain', '--',
                                                                    'test/experiments/buildings'], cwd=root, text=True)),
                implementation_sha256={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                                       for p in paths})
