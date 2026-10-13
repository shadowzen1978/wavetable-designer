"""Independently verify an exported cycle's encoding, length, and amplitudes."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--samples", type=int, choices=(1024, 2048), required=True)
    parser.add_argument("--encoding", choices=("pcm16", "float32"), required=True)
    parser.add_argument("--rate", type=int, choices=(44100, 48000))
    args = parser.parse_args()
    try:
        if args.path.suffix.lower() == ".raw":
            if args.rate is not None:
                parser.error("Raw data contains no sample-rate metadata; omit --rate")
            dtype = np.dtype("<i2" if args.encoding == "pcm16" else "<f4")
            expected_bytes = args.samples * dtype.itemsize
            payload = args.path.read_bytes()
            if len(payload) != expected_bytes:
                raise ValueError(f"Expected {expected_bytes} bytes, found {len(payload)}")
            samples = np.frombuffer(payload, dtype=dtype).astype(np.float64)
            if args.encoding == "pcm16":
                samples /= 32768.0
            report = {"container": "raw", "encoding": args.encoding, "samples": len(samples)}
        else:
            info = sf.info(args.path)
            subtype = "PCM_16" if args.encoding == "pcm16" else "FLOAT"
            if info.format != "WAV" or info.subtype != subtype:
                raise ValueError(f"Expected WAV/{subtype}, found {info.format}/{info.subtype}")
            if info.channels != 1 or info.frames != args.samples:
                raise ValueError(f"Expected mono/{args.samples} frames, found {info.channels} channels/{info.frames}")
            if args.rate is not None and info.samplerate != args.rate:
                raise ValueError(f"Expected {args.rate} Hz, found {info.samplerate}")
            samples, _ = sf.read(args.path, dtype="float64")
            report = {"container": "WAV", "encoding": args.encoding, "samples": info.frames, "rate": info.samplerate}
        if not np.all(np.isfinite(samples)) or np.any(np.abs(samples) > 1.0):
            raise ValueError("Samples must be finite and within [-1, 1]")
        report["peak"] = float(np.max(np.abs(samples)))
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Export check failed: {error}\n")


if __name__ == "__main__":
    main()
