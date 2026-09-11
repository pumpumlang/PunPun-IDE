from __future__ import annotations
import subprocess, sys, tempfile
from pathlib import Path

bridge=Path(sys.argv[1])
with tempfile.TemporaryDirectory() as raw:
    root=Path(raw); dep=root/'dep'; dep.mkdir(); (root/'Punpun.toml').write_text('[package]\nname = "demo"\n\n[dependencies]\n',encoding='utf-8')
    def run(*args): return subprocess.run([str(bridge),*args],cwd=root,text=True,capture_output=True,check=True).stdout
    assert 'added sample' in run('add','sample',str(dep))
    assert 'sample' in run('tree')
    assert '1 path dependency' in run('update')
    assert 'removed sample' in run('remove','sample')
    text=(root/'Punpun.toml').read_text(encoding='utf-8')
    assert text.count('[dependencies]')==1
    assert 'sample =' not in text
print('PPX bridge test passed')
