"""Host regression, sanitizer and optional real-BSP RV32 source checks.
This never links a board ELF, connects hardware, or modifies a BSP.
Override GFX_ROOT, RISCV_CC and SOC_ROOT when moving the repositories.
"""
from pathlib import Path
import json,os,subprocess,shutil
ROOT=Path(__file__).resolve().parents[1]
GFX=Path(os.environ.get('GFX_ROOT',str(ROOT.parent/('software' if (ROOT.parent/'software/include/gfx_draw.h').exists() else 'bitblt-runtime')))).resolve()
OUT=ROOT/'build';OUT.mkdir(exist_ok=True)
report=[]
def run(cmd,env=None):
    result=subprocess.run([str(x) for x in cmd],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    report.append(dict(command=[str(x) for x in cmd],returncode=result.returncode,output=result.stdout))
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(result.stdout,flush=True)
    if result.returncode:raise SystemExit(result.returncode)
run(['make',f'GFX_ROOT={GFX}','test','headless','preview'])
core=['src/game.c','src/backend_control.c'];draw=['src/render.c','src/render_cpu.c','assets/assets.c']
gfx=[str(GFX/p) for p in ['src/error.c','src/draw.c','src/job.c','backends/cpu.c']]
runtime=['platform/gfx_host_runtime.c']
sets=[('game',core+['tests/test_game.c'],[]),('render',core+draw+gfx+runtime+['tests/reference_render.c','tests/test_render.c'],[]),('commands',core+draw+gfx+runtime+['tests/test_commands.c'],[]),('lifecycle',core+draw+gfx+runtime+['tests/test_render_lifecycle.c'],[]),('gfx',gfx+runtime+['tests/test_gfx_common.c'],['-DGFX_TEST_TRACK_ALLOC']),('uart',['platform/board_port.c','tests/test_board_uart.c'],[])]
hardware=[str(GFX/p) for p in ['src/cache_plan.c','src/g2d_assets.c','src/display.c','src/frame_session.c','backends/g2d_v3.c','backends/g2d_stream.c','backends/g2d_job.c','src/input.c','src/input_text.c','src/batch.c','src/runtime_abi.c','src/keys.c','app/loop.c']]
sets.append(('hardware',core+draw+['src/hardware_assets.c','src/hardware_render.c']+gfx+hardware+['tests/reference_render.c','tests/test_hardware_render.c'],['-I'+str(GFX/'app'),'-I'+str(GFX/'checks')]))
sets.append(('backend',['src/backend_control.c','tests/test_backend_control.c'],[]))
for name,sources,extra in sets:
    exe=OUT/('sanitize-'+name)
    run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Iinclude','-I'+str(GFX/'include'),'-I'+str(GFX/'src'),*extra,*sources,'-o',exe])
    run([exe])
run(['python3','tools/uart_bridge.py','--self-test'])
for mode in ('2','3'):
    command=[str(OUT/'danmaku-headless'),'--backend',mode]
    result=subprocess.run(command,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    report.append(dict(command=command,returncode=result.returncode,expected_returncode=6,output=result.stdout))
    if result.returncode!=6:raise RuntimeError('Host accepted a hardware-only backend: '+mode)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
for flags in [[],['--standard'],['--demo','--limit','512']]:
    proc=subprocess.Popen([str(OUT/'danmaku-preview'),*flags],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        output=proc.communicate(timeout=2)[0]
        raise RuntimeError('SDL preview exited early: '+str(proc.returncode)+' '+output)
    except subprocess.TimeoutExpired:
        proc.terminate();output=proc.communicate(timeout=3)[0]
        report.append(dict(command=['SDL dummy preview',*flags],status='ran 2s then test terminated',output=output))
# The toolchain is an existing local dependency, not downloaded here.
riscv=Path(os.environ.get('RISCV_CC',str(GFX/'build/toolchains/xpack-riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf-gcc')))
soc_default=ROOT.parent/'bitblt_soc-main/embedded_sw/newSoC'
if not soc_default.exists():soc_default=ROOT.parent/'bitblt_soc-ysl-dev/embedded_sw/newSoC'
soc=Path(os.environ.get('SOC_ROOT',str(soc_default)))
if riscv.exists() and soc.exists():
    run(['make',f'GFX_ROOT={GFX}','board-check',f'RISCV_CC={riscv}',f'SOC_ROOT={soc}'])
else:
    print('RV32 source check SKIPPED: set RISCV_CC and SOC_ROOT',flush=True)
    report.append(dict(command=['RV32 source check'],status='skipped: missing toolchain/BSP'))
(OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('Validation complete; see build/validation.json',flush=True)
