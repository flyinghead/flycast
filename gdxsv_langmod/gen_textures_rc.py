import os
import sys
import glob
import pathlib

os.chdir(pathlib.Path(sys.path[0]) / "../core/gdxsv")
with open("textures.rc", "w", encoding="utf8") as f:
    for item in map(pathlib.Path, glob.glob("Textures/**/*.png", recursive=True)):
        lang_name = item.parts[1]
        tex_hash = item.stem
        f.write(f'{lang_name.upper()}_{tex_hash.upper()} GDXSV_TEXTURE "{item.as_posix()}"\n')
