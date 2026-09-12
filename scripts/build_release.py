#!/usr/bin/env python3
"""Build PunPun IDE and create a portable test package.

Python is deliberately build tooling only. The shipped IDE executable is native C++.
"""
from __future__ import annotations
import argparse, os, platform, shutil, subprocess, sys, tarfile, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def run(*args: str, cwd: Path = ROOT) -> None:
    print("+", " ".join(map(str,args)))
    subprocess.run(list(map(str,args)), cwd=cwd, check=True)

def main() -> int:
    ap=argparse.ArgumentParser();ap.add_argument("--build-dir",default="build-release");ap.add_argument("--skip-package",action="store_true");ns=ap.parse_args()
    build=ROOT/ns.build_dir;stage=build/"stage";build.mkdir(parents=True,exist_ok=True)
    generator=["-G","Ninja"] if shutil.which("ninja") else []
    run("cmake","-S",ROOT,"-B",build,*generator,"-DCMAKE_BUILD_TYPE=Release","-DBUILD_TESTING=ON")
    run("cmake","--build",build,"--config","Release","--parallel")
    run("ctest","--test-dir",build,"-C","Release","--output-on-failure")
    if stage.exists():shutil.rmtree(stage)
    run("cmake","--install",build,"--config","Release","--prefix",stage)
    if ns.skip_package:return 0
    out=ROOT/"dist";out.mkdir(exist_ok=True)
    system=platform.system().lower()
    if system=="windows":
        exe=next(stage.rglob("punpun-ide.exe"),None)
        if exe and shutil.which("windeployqt"):
            run("windeployqt","--release","--no-translations",exe,cwd=stage)
        archive=out/"PunPun-IDE-v0.5.0-windows-x64.zip"
        if archive.exists():archive.unlink()
        with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED) as z:
            for p in stage.rglob("*"):
                if p.is_file():z.write(p,p.relative_to(stage))
    else:
        archive=out/"PunPun-IDE-v0.5.0-linux-x86_64.tar.gz"
        with tarfile.open(archive,"w:gz") as t:t.add(stage,arcname="PunPun-IDE")
    print("Created",archive)
    return 0
if __name__=="__main__":raise SystemExit(main())
