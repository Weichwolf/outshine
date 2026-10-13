"""Render the current closer city cameras against the complete cached building context."""

import argparse
from pathlib import Path

from evidence import revision
from showcase_scene import LABELS,cached
from showcase_snapshot import urban_views


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/cities-close'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    base=Path('build/experiments/buildings');provenance=revision()
    bodies=[cached(label,base/'opening-inspections',base/'roof-plans') for label in LABELS]
    for site in ('Vienna','Tokyo','CentralPark'):
        focal=[body for body in bodies if body['plan']['id'].startswith(site+':')]
        urban_views(site,focal,args.output,provenance,False)


if __name__=='__main__':main()
