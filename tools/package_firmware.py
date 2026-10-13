"""Package only locally built images; never touches a serial port."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

root=Path(__file__).resolve().parents[1]
size_tool=Path.home()/'.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size.exe'
if not size_tool.exists():
    size_tool=Path(shutil.which('xtensa-esp32-elf-size') or 'xtensa-esp32-elf-size')
manifest={'built_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'board':'esp32doit-devkit-v1','flash_configured_bytes':4194304,'app_partition_bytes':3145728,'full_hardware_acceptance_passed':False,'hardware_evidence':'docs/TESTING.md','images':{}}
for env in ['esp32-oled','esp32-single']:
    source=root/'.pio/build'/env
    destination=root/'firmware'/env
    destination.mkdir(parents=True,exist_ok=True)
    result=subprocess.check_output([str(size_tool),'-A',str(source/'firmware.elf')],text=True)
    sections={}
    for line in result.splitlines():
        fields=line.split()
        if len(fields)>=3 and fields[0].startswith('.') and fields[1].isdigit():
            sections[fields[0]]=int(fields[1])
    # Match the pinned PlatformIO ESP32 builder's accounting, not generic ELF
    # text/data/bss totals (which include other address spaces and metadata).
    flash=sum(sections.get(s,0) for s in ['.iram0.text','.iram0.vectors','.dram0.data','.flash.text','.flash.rodata'])
    ram=sum(sections.get(s,0) for s in ['.dram0.data','.dram0.bss','.noinit'])
    entry={'linked_flash_bytes':flash,'static_ram_bytes':ram,'files':{}}
    for filename,offset in [('bootloader.bin','0x1000'),('partitions.bin','0x8000'),('boot_app0.bin','0xe000'),('firmware.bin','0x10000')]:
        image=source/filename
        if filename=='boot_app0.bin':
            image=Path.home()/'.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
        content=image.read_bytes()
        shutil.copyfile(image,destination/filename)
        entry['files'][filename]={'offset':offset,'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()}
    manifest['images'][env]=entry
(root/'docs/build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
