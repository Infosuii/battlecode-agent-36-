"""Build an Agent 2 variant: bots/NAME = bots/SRC/src with switch defines prepended to common.h, release build,
source and wasm hashes appended to agent2/builds.tsv.

usage: python -I agent2/tools/mkvar.py NAME [DEF=VAL ...] [--src a2_v36]
"""
import hashlib
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def src_hash(d):
    h = hashlib.sha256()
    for f in sorted(d.glob("*")):
        h.update(f.name.encode() + b"\0" + f.read_bytes() + b"\0")
    return h.hexdigest()


def main():
    args = sys.argv[1:]
    src = args[args.index("--src") + 1] if "--src" in args else "a2_v36"
    rest = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] != "--src")]
    name, defs = rest[0], rest[1:]
    d = ROOT / "bots" / name
    if d.exists():
        shutil.rmtree(d)
    shutil.copytree(ROOT / "bots" / src / "src", d / "src")
    ch = d / "src" / "common.h"
    text = ch.read_text(encoding="utf-8-sig")
    pre = "".join(f"#define {k} {v}\n" for k, v in (x.split("=", 1) for x in defs))
    ch.write_text(f"// {name}: {src} + {' '.join(defs) or '(no overrides)'}\n" + pre + text, encoding="utf-8")
    r = subprocess.run([str(ROOT / "cambc.exe"), "build", f"bots/{name}", "--release"], cwd=ROOT, capture_output=True, text=True)
    out = r.stdout + r.stderr
    wasm = d / ".cambc" / "build-release" / "bot.wasm"
    if " error:" in out or "error: " in out.split("injected")[0][-4000:] or not wasm.exists():
        print(out[-4000:])
        sys.exit(f"build failed: {name}")
    sh, wh = src_hash(d / "src"), hashlib.sha256(wasm.read_bytes()).hexdigest()
    log = ROOT / "agent2" / "builds.tsv"
    if not log.exists():
        log.write_text("time\tname\tsrc\tdefines\tsource_sha256\twasm_sha256\n", encoding="utf-8")
    with log.open("a", encoding="utf-8") as f:
        f.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')}\t{name}\t{src}\t{' '.join(defs)}\t{sh}\t{wh}\n")
    print(f"{name}: source {sh[:12]} wasm {wh[:12]}")


if __name__ == "__main__":
    main()
