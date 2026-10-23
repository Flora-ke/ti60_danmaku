from pathlib import Path
import subprocess, os, json
root=Path(__file__).resolve().parents[1]
report=[]
def run(cmd, **kw):
    p=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,**kw)
    report.append({'command':cmd,'returncode':p.returncode,'output':p.stdout})
    print(p.stdout,flush=True)
    if p.returncode: raise SystemExit(p.returncode)
run(['make','board-check','RISCV_CC=/mnt/d/Desktop/2026FPGA/software/build/toolchains/xpack-riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf-gcc','SOC_ROOT=/mnt/d/Desktop/2026FPGA/bitblt_soc-main/embedded_sw/newSoC'])
for name,sources in [('game',['src/game.c','tests/test_game.c']),('render',['src/game.c','src/render.c','assets/assets.c','tests/test_render.c']),('uart',['platform/board_port.c','tests/test_board_uart.c'])]:
    exe='build/sanitize-'+name
    run(['gcc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Iinclude',*sources,'-o',exe])
    run(['./'+exe])
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
p=subprocess.Popen(['./build/danmaku-preview'],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
try:
    output=p.communicate(timeout=2)[0]
    report.append({'command':['SDL_dummy_preview'],'returncode':p.returncode,'output':output})
    raise SystemExit('Preview exited unexpectedly')
except subprocess.TimeoutExpired:
    p.terminate();output=p.communicate(timeout=3)[0]
    report.append({'command':['SDL_dummy_preview'],'status':'ran_2s_then_terminated','output':output})
(root/'build'/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
print('RV32 default/RAM syntax, ASan/UBSan and 2-second SDL dummy smoke: PASS')
