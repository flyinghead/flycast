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

N = int(os.getenv("N", 4))
ROM = os.getenv("ROM", r"C:\rom\gdx-disc2\gdx-disc2.gdi")
FLYCAST = os.getenv("FLYCAST", r"R:\Temp\flycast.exe")
FLYCAST_NAME = Path(FLYCAST).name
X_OFFSET = 0
Y_OFFSET = 50
W = 640
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


def run(idx, *arg_list) -> subprocess.Popen[str]:
    cmd = " ".join(arg_list)
    print(cmd)
    new_env = os.environ.copy()
    new_env["GGPO_NETWORK_DELAY"] = "16"
    # else: new_env["GGPO_NETWORK_DELAY"] = "100"
    return subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=new_env)


def run_rom(idx: int) -> subprocess.Popen[str]:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_log(idx),
        conf_window_layout(idx),
    )


def run_replay(idx: int) -> subprocess.Popen[str]:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_window_layout(idx),
        conf_log(idx),
    )


def run_rbk_test(idx: int) -> subprocess.Popen[str]:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_window_layout(idx),
        conf_log(idx),
        f"--config gdxsv:rbk_test={idx+1}/{N} --config gdxsv:rand_input=12345"
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
        while popens[0].poll() == None:
            time.sleep(1)
    finally:
        for p in popens:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/F', '/T', '/PID', str(p.pid)])
            else:
                p.kill()
        for p in popens:
            print(f"exit {p.poll()} {p.args}")


if __name__ == "__main__":
    main()

