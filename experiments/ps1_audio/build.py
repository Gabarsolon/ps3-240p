"""Offline build of the isolated, stock-output audio-capture control."""
from pathlib import Path
import os,sys,json,subprocess,hashlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from build_plugin import SDK,SIBLING,CFLAGS,MODULE_INFO_MINOR
OUT=ROOT/'build/ps1-audio/plugin';OUT.mkdir(parents=True,exist_ok=True)
elf=OUT/'crt_ps1_audio.elf';prx=elf.with_suffix('.prx');sprx=elf.with_suffix('.sprx')
env=dict(os.environ);env['PS3DK']=str(SDK)
def run(args):subprocess.run([str(x) for x in args],cwd=ROOT,env=env,check=True)
run([SDK/'ppu/bin/ppu-gcc.exe',*CFLAGS,f'-specs={SDK / "ppu/lib/lv2-prx.specs"}',
 '-nostartfiles','-Wl,-q','-Wl,-z,max-page-size=16',f'-Wl,-T,{(SIBLING / "game/native-prx.ld").as_posix()}',
 '-Wl,--gc-sections',ROOT/'experiments/ps1_audio/probe.c',ROOT/'experiments/ps1_audio/pcm.c',ROOT/'plugin/vsh_video.c',SDK/'ppu/lib/lv2-prx-crt.o',
 '-lio','-o',elf])
run([sys.executable,'-B',SIBLING/'tools/native_prx.py',elf,prx,'--name','crt_ps1_audio','--start','module_start','--stop','module_stop'])
layout=json.loads(Path(str(prx)+'.layout.json').read_text());data=bytearray(prx.read_bytes());data[layout['module_info_file_offset']+3]=MODULE_INFO_MINOR;prx.write_bytes(data)
run([sys.executable,'-B',SIBLING/'tools/sign_native_self.py',prx,sprx,'--vsh'])
print(json.dumps({'path':str(sprx),'bytes':sprx.stat().st_size,'sha256':hashlib.sha256(sprx.read_bytes()).hexdigest()},indent=2))
