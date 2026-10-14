"""Check native smoke exports against their saved authored projects using NumPy/libsndfile."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?", default=ROOT / "build/gui-smoke/files")
    args = parser.parse_args()
    checked = 0
    for count in (1024, 2048):
        for profile in (1, 2):
            for fmt in (1, 2, 3, 4):
                if profile == 1 and fmt in (2, 4):
                    continue
                for rate in (44100, 48000):
                    raw = fmt >= 3
                    file = args.directory / f"{count}-{profile}-{fmt}-{rate}.{'raw' if raw else 'wav'}"
                    command = [
                        sys.executable, str(ROOT / "tools/check_export.py"), str(file),
                        "--samples", str(1024 if profile == 1 else count),
                        "--encoding", "pcm16" if fmt in (1, 3) else "float32",
                        "--project", str(args.directory / f"{count}.wtd"),
                        "--profile", "sz404" if profile == 1 else "generic",
                    ]
                    if not raw:
                        command.extend(("--rate", str(rate)))
                    result = subprocess.run(command, capture_output=True, text=True)
                    if result.returncode:
                        raise RuntimeError(f"{file.name}: {result.stderr.strip()}")
                    checked += 1
    print(f"Independent validation passed: {checked} exports; format, rate, mono length, endian and authored sample agreement.")

if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        raise SystemExit(str(error)) from error
