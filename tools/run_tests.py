"""Compile the ACTUAL portable firmware modules and run independent Python oracles."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
os.chdir(root)
zig=root/'tools/host/Lib/site-packages/ziglang/zig.exe'
compiler=[str(zig),'c++'] if zig.exists() else [shutil.which('g++') or 'g++']
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(root/'build/zig-cache')
os.environ['ZIG_LOCAL_CACHE_DIR']=str(root/'build/zig-local')
headers=root/'.pio/libdeps/esp32-oled/ArduinoJson/src'
if not headers.exists():
    raise SystemExit('Build esp32-oled with PlatformIO first to install ArduinoJson.')
Path('build').mkdir(exist_ok=True)
exe='build/core_tests.exe' if os.name=='nt' else 'build/core_tests'
sources=list(Path('lib/MiningCore/src').glob('*.cpp'))+[Path('tests/core_cli.cpp')]
command=compiler+['-std=c++17','-O2','-fno-strict-aliasing','-Ilib/MiningCore/src',f'-I{headers}',*map(str,sources),'-o',exe]
result=subprocess.run(command)
if result.returncode:
    raise SystemExit(result.returncode)
os.environ['MINER_CORE_EXE']=str(root/exe)
result=subprocess.run([sys.executable,'-m','unittest','discover','-s','tests','-v'])
raise SystemExit(result.returncode)
