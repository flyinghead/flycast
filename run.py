#!/usr/bin/env python3
import functools
import os
import subprocess
import shutil
import sys
import glob
import random
import time
import threading
import urllib.request
import shutil
from typing import List
from os.path import exists
from pathlib import Path

print = functools.partial(print, flush=True)
os.chdir(os.path.dirname(os.path.abspath(__file__)))

N = int(os.getenv("N", 4))
TIMEOUT = int(os.getenv("TIMEOUT", 3600))
ITERATION = int(os.getenv("ITERATION", 1))
WIDE = int(os.getenv("WIDE", 0))
ROM = os.getenv("ROM", r"C:\rom\gdx-disc2\gdx-disc2.gdi")
FLYCAST = os.getenv("FLYCAST", r"R:\Temp\flycast.exe")
FLYCAST_NAME = Path(FLYCAST).name
X_OFFSET = 0
Y_OFFSET = 50
W = 854 if WIDE else 640
H = 480

def download_state():
    os.makedirs(f"work/state", exist_ok=True)
    if not os.path.isfile(f"work/state/gdx-disc2_99.state"):
        urllib.request.urlretrieve("https://storage.googleapis.com/gdxsv/misc/gdx-disc2_99.state", f"work/state/gdx-disc2_99.state")


def download_flycast(url: str):
    cwd = os.getcwd()
    os.makedirs(f"work", exist_ok=True)
    os.chdir("work")

    if exists("flycast.exe"):
        os.remove("flycast.exe")

    save_name = os.path.basename(url)
    urllib.request.urlretrieve(url, save_name)

    if save_name.endswith("zip"):
        shutil.unpack_archive(save_name, ".")

    if exists("flycast-gdxsv.exe"):
        os.rename("flycast-gdxsv.exe", "flycast.exe")
    
    assert exists("flycast.exe")

    global FLYCAST
    global FLYCAST_NAME
    FLYCAST = "work/flycast.exe"
    FLYCAST_NAME = "flycast.exe"

    os.chdir(cwd)


def prepare_workdir(idx: int):
    os.makedirs(f"work/flycast{idx}/data", exist_ok=True)
    shutil.copy(Path(FLYCAST), f"work/flycast{idx}/")
    for file in glob.glob(os.path.join("work/state", "*.state")):
        if os.path.isfile(file):
            shutil.copy(file, f"work/flycast{idx}/data")
    return f"work/flycast{idx}"


def conf_log(idx: int):
    return f"--config log:Verbosity={1} --config log:LogToFile=1"


def conf_volume(idx: int):
    return f"--config config:aica.Volume=20"


def conf_gdxsv(idx: int):
    return f"--config gdxsv:server=127.0.0.1"


def conf_window_layout(idx: int):
    x = X_OFFSET + W * (idx % 2)
    y = Y_OFFSET + H * (idx // 2)
    wide = "yes" if WIDE else "no"
    return f"--config window:top={y} --config window:left={x} --config window:width={W} --config window:height={H} --config config:rend.WideScreen={wide} --config config:rend.WidescreenGameHacks={wide}"


def run(idx, *arg_list) -> subprocess.Popen:
    cmd = " ".join(arg_list)
    print(cmd)
    new_env = os.environ.copy()
    new_env["GGPO_NETWORK_DELAY"] = "16"
    new_env["GGPO_OOP_PERCENT"] = "1"
    # else: new_env["GGPO_NETWORK_DELAY"] = "100"
    return subprocess.Popen(cmd, shell=True, env=new_env)


def run_rom(idx: int) -> subprocess.Popen:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_volume(idx),
        conf_log(idx),
        conf_window_layout(idx),
    )


def run_replay(idx: int) -> subprocess.Popen:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_volume(idx),
        conf_window_layout(idx),
        conf_log(idx),
    )


def run_rbk_test(idx: int) -> subprocess.Popen:
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_volume(idx),
        conf_window_layout(idx),
        conf_log(idx),
        f"--config gdxsv:rbk_test={idx+1}/{N}"
    )


def run_rbk_test_random(idx: int) -> subprocess.Popen:
    seed = random.randint(1, 99999)
    return run(idx,
        FLYCAST_NAME, ROM,
        conf_gdxsv(idx),
        conf_volume(idx),
        conf_window_layout(idx),
        conf_log(idx),
        f"--config gdxsv:rbk_test={idx+1}/{N} --config gdxsv:rand_input={seed}"
    )


def truncate(path: str):
    with open(path, "w") as f:
        f.truncate(0)


def tail(parent: subprocess.Popen, path: str):
    with open(path, "r") as f:
        f.seek(0, 2)
        while parent.poll() == None:
            print(f.readline(), end="")


def exec_func(func_name: str):
    start_time = time.time()
    cwd = os.getcwd()
    print(cwd)

    func = {f.__name__[4:]: f for f in [
        run_rom,
        run_replay,
        run_rbk_test,
        run_rbk_test_random,
    ]}[func_name]

    popens: List[subprocess.Popen] = []

    try:
        for i in reversed(range(N)):
            wdir = prepare_workdir(i+1)
            os.chdir(wdir)
            truncate("flycast.log")
            p = func(i)
            if i == 0:
                threading.Thread(target=tail, args=(p, "flycast.log"), name="tail", daemon=True).start()
            popens.append(p)
            os.chdir(cwd)

        while popens[-1].poll() == None:
            time.sleep(1)
            if TIMEOUT < time.time() - start_time:
                print("timeout")
                break
    finally:
        for p in popens:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/F', '/T', '/PID', str(p.pid)])
            else:
                p.kill()
        for p in popens:
            print(f"exit {p.poll()} {p.args}")


def main():
    if FLYCAST.startswith("http"):
        download_flycast(FLYCAST)
    
    download_state()

    for t in range(ITERATION):
        print(f"===== ITERATION {t + 1}/{ITERATION} START =====")
        exec_func(sys.argv[1])
        print(f"===== ITERATION {t + 1}/{ITERATION} END   =====")


if __name__ == "__main__":
    main()
