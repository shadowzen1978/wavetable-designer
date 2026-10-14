"""Run the actual SZ404 importer using existing Windows Release libraries; optional local check."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
from dev import ROOT, build_environment

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT.parent / "ts404-remake")
    parser.add_argument("--sz404-build", type=Path)
    parser.add_argument("--fixtures", type=Path, default=ROOT / "build/gui-smoke/files")
    args = parser.parse_args()
    sister = args.repo.resolve()
    reference = (args.sz404_build or sister / "build").resolve()
    build = ROOT / "build/sz404-import-check"
    env = build_environment()
    for command in (
        ["cmake", "-S", str(ROOT / "tools/sz404-import-check"), "-B", str(build),
         "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-DSZ404_REPO={sister}", f"-DSZ404_BUILD={reference}"],
        ["cmake", "--build", str(build), "--parallel", "4"],
        [str(build / "wtd_sz404_import_check.exe"), str(args.fixtures.resolve())],
    ):
        subprocess.run(command, cwd=ROOT, env=env, check=True)

if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
