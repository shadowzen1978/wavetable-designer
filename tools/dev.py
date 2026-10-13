"""Portable CMake driver. Run through mise so pinned tools are on PATH."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build_environment() -> dict[str, str]:
    env = os.environ.copy()
    if os.name != "nt":
        return env
    installer = Path(env.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer"
    vswhere = installer / "vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError("Install Visual Studio Build Tools with the C++ workload")
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-find",
         "VC/Auxiliary/Build/vcvarsall.bat"],
        check=True, capture_output=True, text=True,
    )
    candidates = result.stdout.strip().splitlines()
    if not candidates:
        raise RuntimeError("vswhere found no x64 C++ build tools")
    env["PATH"] = str(installer) + os.pathsep + env.get("PATH", "")
    env["WTD_VCVARSALL"] = candidates[0]
    result = subprocess.run(
        ["cmd.exe", "/d", "/c", str(ROOT / "tools/msvc-env.cmd")],
        env=env, check=True, capture_output=True, text=True,
    )
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if separator and key and not key.startswith("="):
            env[key.upper()] = value
    if not shutil.which("cl.exe", path=env.get("PATH")):
        raise RuntimeError("MSVC environment setup did not provide cl.exe")
    return env


def execute(args: list[str], env: dict[str, str], **kwargs) -> None:
    print("+ " + " ".join(args), flush=True)
    subprocess.run(args, cwd=ROOT, env=env, check=True, **kwargs)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "build-core", "test", "run", "smoke"))
    args = parser.parse_args()
    host = {"Windows": "windows", "Darwin": "macos", "Linux": "linux"}.get(platform.system())
    if host is None:
        parser.error("Unsupported build host")
    core_only = args.action in ("build-core", "test")
    if host == "linux" and not core_only:
        parser.error("Linux currently supports only the portable core harness")
    preset = ("core-" if core_only else "app-") + host
    env = build_environment()
    execute(["cmake", "--preset", preset], env)
    execute(["cmake", "--build", "--preset", preset, "--parallel", "4"], env)
    if args.action == "test":
        execute(["ctest", "--preset", preset], env)
    if args.action in ("run", "smoke"):
        artefacts = ROOT / "build" / preset / "WavetableDesigner_artefacts" / "Release"
        executable = (artefacts / "Wavetable Designer.exe") if host == "windows" else (
            artefacts / "Wavetable Designer.app/Contents/MacOS/Wavetable Designer")
        if args.action == "smoke":
            execute([str(executable), "--smoke-test"], env, timeout=20)
        elif host == "macos":
            execute(["open", str(artefacts / "Wavetable Designer.app")], env)
        else:
            subprocess.Popen([str(executable)], cwd=ROOT, env=env)
            print(f"Launched {executable}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise SystemExit(str(error)) from error
