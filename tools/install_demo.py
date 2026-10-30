"""Copy the owned MV-format demo to games/ without overwriting an existing folder."""
from pathlib import Path
import argparse
import shutil

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--home',type=Path,default=root)
args=parser.parse_args()
target=args.home/'games/RPG2LX Demo'
if target.exists():
    parser.exit(0,f'Demo already exists; left unchanged: {target}\n')
shutil.copytree(root/'fixtures/playable_project',target)
print(target)
