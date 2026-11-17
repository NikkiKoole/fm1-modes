#!/usr/bin/env python3
"""macOS/Linux port of tools/Build-Web.ps1 -LocalPreview.

Compiles the browser simulator (app/ + web/ + third_party/synth) to WASM with
WASI SDK 27 and embeds it into build/preview/index.html.

Usage:  python3 tools/build_web_mac.py [--wasi-sdk PATH]
"""
import argparse, base64, hashlib, html, json, os, re, subprocess, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
ap = argparse.ArgumentParser()
ap.add_argument("--wasi-sdk", default=os.environ.get("FM1_WASI_SDK")
                or str(root / "build/tools/wasi-sdk-27.0-arm64-macos"))
args = ap.parse_args()
wasi = Path(args.wasi_sdk)
clang = wasi / "bin/clang"
if not clang.exists():
    sys.exit(f"WASI SDK not found at {wasi} (set FM1_WASI_SDK)")

sha = lambda b: hashlib.sha256(b).hexdigest()

# Same source selection as the PowerShell script.
sources = []
for d in ("app", "web", "third_party/synth"):
    for f in sorted((root / d).rglob("*.c")):
        rel = f.relative_to(root).as_posix()
        if not rel.startswith("app/board/"):
            sources.append(str(f))

out = root / "build/web"; out.mkdir(parents=True, exist_ok=True)
wasm = out / "simulator.wasm"
cmd = [str(clang), f"--sysroot={wasi/'share/wasi-sysroot'}",
       "-std=c11", "-O2", "-fwrapv", "-fno-fast-math", "-ffp-contract=off",
       "-DFM1_HOST_TEST", "-DFM1_OPEN_WEB", "-Wall", "-Wextra", "-Werror",
       "-Wno-unused-function", "-Wno-array-parameter", "-mexec-model=reactor"]
cmd += [f"-I{root/d}" for d in ("web/engine", "web/synth", "third_party/synth",
                                 "sdk/include", "app/synth", "app")]
cmd += sources
cmd += ["-Wl,--strip-all", "-Wl,-z,stack-size=131072",
        "-Wl,--initial-memory=4194304", "-Wl,--max-memory=33554432", "-o", str(wasm)]
print("Compile the browser application")
subprocess.run(cmd, check=True)

version = re.search(r'(?m)^#define FM1_RELEASE_VERSION "(V\d+)"\s*$',
                    (root / "app/version.h").read_text()).group(1)
wbytes = wasm.read_bytes()
src_hash = sha("\n".join(sorted(sources)).encode())
simulation = json.dumps({"schema": 1, "release_version": version, "sha256": sha(wbytes),
                         "source_sha256": src_hash, "sample_bank": "synthesized-demo",
                         "data": base64.b64encode(wbytes).decode()}, separators=(",", ":"))

page = (root / "sdk/Update.html.in").read_text(encoding="utf-8")
page = re.sub(r'<a\b[^>]*href="/\*FM1_SOURCE_URL\*/"[^>]*>(.*?)</a>',
              r'<span>\1 (local preview)</span>', page)
notices = ["APP: GPLv3 with the FM1 SDK linking exception; no warranty. SDK and firmware "
           "resources have separate redistribution terms in THIRD_PARTY.md. Source: "]
for f in ("THIRD_PARTY.md", "LICENSE.md"):
    notices.append(f"\n===== {f} =====\n" + (root / f).read_text(encoding="utf-8"))
rep = {
    "/*FM1_SOURCE_URL*/": "",
    "/*FM1_PAYLOAD*/": "const FM1_PACKAGE = null;",
    "/*FM1_SIMULATOR*/": "const FM1_SIMULATOR = " + simulation + ";",
    "/*FM1_I18N*/": (root / "sdk/I18n.js").read_text(encoding="utf-8"),
    "/*FM1_SCRIPT*/": (root / "sdk/WebUpdate.js").read_text(encoding="utf-8"),
    "/*FM1_SIM_SCRIPT*/": (root / "sdk/Simulator.js").read_text(encoding="utf-8"),
    "/*FM1_SAMPLE_SCRIPT*/": (root / "sdk/Samples.js").read_text(encoding="utf-8"),
    "/*FM1_NOTICES*/": html.escape("\n".join(notices), quote=True),
}
for k, v in rep.items():
    page = page.replace(k, v)
if re.search(r"/\*FM1_[A-Z_]+\*/", page):
    sys.exit("Unexpanded HTML placeholder")
dest = root / "build/preview/index.html"
dest.parent.mkdir(parents=True, exist_ok=True)
dest.write_text(page, encoding="utf-8")
print(f"Preview: {dest}  ({len(wbytes)} byte wasm)")
