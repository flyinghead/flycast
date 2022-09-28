#!/usr/bin/env python3
import functools
import os
import subprocess
import shutil
import sys
import glob
import time
import threading
from pathlib import Path

print = functools.partial(print, flush=True)
os.chdir(os.path.dirname(os.path.abspath(__file__)))

N = os.getenv("N", 4)
ROM = os.getenv("ROM", r"C:\data\rom\gdx-disc2\gdx-disc2.gdi")
FLYCAST = os.getenv("FLYCAST", "out/build/x64-Debug/flycast.exe")
FLYCAST_NAME = Path(FLYCAST).name
X_OFFSET = 0
Y_OFFSET = 50
W = 480
H = 480


def prepare_workdir(idx: int):
    os.makedirs(f"work/flycast{idx}/data", exist_ok=True)
    shutil.copy(Path(FLYCAST), f"work/flycast{idx}/")
    for file in glob.glob(os.path.join("work/state", "*.state")):
        if os.path.isfile(file):
            shutil.copy(file, f"work/flycast{idx}/data")
    return f"work/flycast{idx}"


def conf_log(idx: int):
    return f"--config log:Verbosity={1} --config log:LogToFile=1"


def conf_gdxsv(idx: int):
    return f"--config gdxsv:server=127.0.0.1"


def conf_window_layout(idx: int):
    x = X_OFFSET + W * (idx % 2)
    y = Y_OFFSET + H * (idx // 2)
    return f"--config window:top={y} --config window:left={x} --config window:width={W} --config window:height={H}"


def run(*arg_list) -> subprocess.Popen[str]:
    cmd = " ".join(arg_list)
    print(cmd)
    return subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def run_rom(idx: int) -> subprocess.Popen[str]:
    return run(
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_log(idx),
        conf_window_layout(idx),
    )


def run_replay(idx: int) -> subprocess.Popen[str]:
    return run(
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_window_layout(idx),
        conf_log(idx),
    )


def run_rbk_test(idx: int) -> subprocess.Popen[str]:
    return run(
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_window_layout(idx),
        conf_log(idx),
        f"--config gdxsv:rbk_test={idx+1}"
    )


def truncate(path: str):
    with open(path, "w") as f:
        f.truncate(0)


def tail(path: str):
    with open(path, "r") as f:
        f.seek(0, 2)
        while True:
            print(f.readline(), end="")


def main():
    cwd = os.getcwd()
    print(cwd)

    run_funcs = {f.__name__[4:]: f for f in [
        run_rom,
        run_replay,
        run_rbk_test,
    ]}

    popens: [subprocess.Popen[str]] = []

    try:
        for i in reversed(range(N)):
            wdir = prepare_workdir(i+1)
            os.chdir(wdir)
            truncate("flycast.log")
            if i == 0:
                threading.Thread(target=tail, args=("flycast.log",), name="tail", daemon=True).start()
            p = run_funcs[sys.argv[1]](i)
            popens.append(p)
            os.chdir(cwd)
        time.sleep(24*60*60)
    finally:
        for p in popens:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/F', '/T', '/PID', str(p.pid)])
            else:
                p.kill()


if __name__ == "__main__":
    main()

