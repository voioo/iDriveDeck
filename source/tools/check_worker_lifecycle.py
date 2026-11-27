"""Check reaper coverage; optionally prove the tests reject a pinned old source tree.

This enumerates plugins containing waitpid calls, not occurrences of fix text.
The C++ harness exercises actual production methods with controlled wait results.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / "native"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-root", type=Path,
                        help="old repository checkout whose production reapers must fail the live-PID case")
    args = parser.parse_args()
    names = (ROOT / "tests/lifecycle-plugins.txt").read_text().split()
    actual = {path.stem for path in (ROOT / "src/plugins").glob("*.cpp")
              if re.search(r"\bwaitpid\s*\(", path.read_text())}
    if len(names) != len(set(names)) or set(names) != actual:
        raise SystemExit(f"Reaper test coverage differs: missing={actual-set(names)}, stale={set(names)-actual}")
    print(f"Production worker reaper coverage: {len(names)} plugins (including both BoostDeck processes)")
    if not args.baseline_root:
        return
    baseline = args.baseline_root.resolve() / "source/native/src/plugins"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    with tempfile.TemporaryDirectory(prefix="idrive-reap-before-") as directory:
        for name in names:
            source = baseline / f"{name}.cpp"
            if name == "dpf_deck" and not source.is_file():
                # DPFDeck was introduced after the pinned original baseline.
                # Candidate coverage is still required by the inventory above.
                print("Baseline dpf_deck: absent; covered by candidate ownership tests")
                continue
            output = Path(directory) / name
            command = compiler + ["-std=gnu++98", "-fno-access-control",
                *[f"-I{ROOT/path}" for path in ("sdk", "src/common", "src/ipc", "src/plugins", "tests")],
                f"-DTEST_{name}=1", "-DLIFECYCLE_SOURCE="+json.dumps(str(source)),
                str(ROOT/"tests/test_worker_lifecycle.cpp"), str(ROOT/"tests/fake_bmw.cpp"),
                str(ROOT/"build/lifecycle-core.o"), str(ROOT/"build/launch_timer.o"), "-lm", "-o", str(output)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            if compiled.returncode:
                raise SystemExit(f"Baseline did not compile for {name}:\n{compiled.stderr}")
            tested = subprocess.run([output], capture_output=True, text=True, timeout=10)
            if tested.returncode != 1 or "lost a live/uncertain worker" not in tested.stderr:
                raise SystemExit(f"Baseline {name} did not demonstrate the expected ownership failure: {tested.returncode}\n{tested.stderr}")
            print(f"Baseline {name}: rejected for forgetting a live worker")
            if name in {"boost_deck", "shift_deck", "warm_deck", "engine_codes"}:
                controlled = subprocess.run([output, "--controller-only"], capture_output=True, text=True, timeout=10)
                if controlled.returncode != 1 or "duplicate worker launched" not in controlled.stderr:
                    raise SystemExit(f"Baseline {name} did not reproduce duplicate launch: {controlled.returncode}\n{controlled.stderr}")
                print(f"Baseline {name}: duplicate launch reproduced after retry deadline")


if __name__ == "__main__":
    main()
