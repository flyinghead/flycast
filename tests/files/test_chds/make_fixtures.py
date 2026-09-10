#!/usr/bin/env python3
"""Regenerates the .cue and .chd test images sitting in this directory.

    ./make_fixtures.py            (needs chdman from mame-tools)

Each sector of the generated .bin files starts with the 8 bytes

    { track, 0x11, frame & 0xff, 0x22, frame >> 8, 0x33, 0x44, 0x55 }

and is zero filled afterwards, so a sector identifies the track and the frame
it belongs to. Swapping the marker byte pairs gives a different value, which is
what lets ChdTest catch a missing or spurious byte swap of the audio tracks.

The .bin files are deleted once chdman has read them: they are 8 MB of filler
and everything needed to rebuild them is here.
"""
import os
import shutil
import subprocess
import sys

# name -> (cue header, [(track number, type, pregap frames, data frames)])
# The pregap frames are stored in the track's own .bin, ie. the track has an
# INDEX 00, unless the layout says otherwise below.
FIXTURES = {
    # MIL-CD laid out as Redump dumps them: the audio tracks of the first
    # session carry their pregap, the data track of the second one doesn't.
    "milcd": [
        ("REM SESSION 01", 1, "AUDIO", 0, 300),
        (None, 2, "AUDIO", 183, 300),
        (None, 3, "AUDIO", 210, 200),
        ("REM SESSION 02", 4, "MODE2/2352", 0, 450),
    ],
    # Same disc, but the pregap of the second session's track is in the image
    "milcd_pregap": [
        ("REM SESSION 01", 1, "AUDIO", 0, 300),
        ("REM SESSION 02", 2, "MODE2/2352", 150, 300),
    ],
    # Same disc again, with that pregap declared but not stored
    "milcd_nogap": [
        ("REM SESSION 01", 1, "AUDIO", 0, 300),
        ("REM SESSION 02", 2, "MODE2/2352", -150, 300),
    ],
    # Single session audio CD: no session gap must be inserted
    "audiocd": [
        (None, 1, "AUDIO", 0, 150),
        (None, 2, "AUDIO", 0, 150),
        (None, 3, "AUDIO", 0, 150),
    ],
}

SECTOR = 2352


def msf(frames):
    return "%02d:%02d:%02d" % (frames // (75 * 60), frames // 75 % 60, frames % 75)


def write_bin(path, track, frames):
    with open(path, "wb") as f:
        for i in range(frames):
            sector = bytearray(SECTOR)
            sector[0:8] = bytes([track, 0x11, i & 0xFF, 0x22, i >> 8, 0x33, 0x44, 0x55])
            f.write(sector)


def build(name, tracks, chdman):
    bins = []
    cue = []
    for header, track, track_type, pregap, frames in tracks:
        if header:
            cue.append(header)
        # A negative pregap is declared with a PREGAP command instead of an
        # INDEX 00, ie. those sectors take up space on the disc but aren't stored
        stored_pregap = pregap if pregap > 0 else 0
        binname = "%s%02d.bin" % (name, track)
        write_bin(os.path.join(os.path.dirname(__file__) or ".", binname),
                  track, stored_pregap + frames)
        bins.append(binname)
        cue.append('FILE "%s" BINARY' % binname)
        cue.append("  TRACK %02d %s" % (track, track_type))
        if pregap < 0:
            cue.append("    PREGAP %s" % msf(-pregap))
        if stored_pregap:
            cue.append("    INDEX 00 00:00:00")
        cue.append("    INDEX 01 %s" % msf(stored_pregap))

    here = os.path.dirname(__file__) or "."
    cuepath = os.path.join(here, name + ".cue")
    with open(cuepath, "w") as f:
        f.write("\n".join(cue) + "\n")

    chdpath = os.path.join(here, name + ".chd")
    subprocess.run([chdman, "createcd", "-i", cuepath, "-o", chdpath, "-f"],
                   check=True, stdout=subprocess.DEVNULL)
    for binname in bins:
        os.remove(os.path.join(here, binname))
    print("%s: %d tracks, %d bytes" % (name + ".chd", len(tracks), os.path.getsize(chdpath)))


def main():
    chdman = shutil.which("chdman")
    if chdman is None:
        sys.exit("chdman not found (apt install mame-tools)")
    for name, tracks in FIXTURES.items():
        build(name, tracks, chdman)


if __name__ == "__main__":
    main()
