#!/usr/bin/env python3
"""Build a RAM-loadable game ELF against a fingerprinted, read-only Sapphire BSP.
No serial/JTAG connection, reset, flash programming or hardware writes.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gfx-root',type=Path,default=ROOT.parent/'bitblt-runtime')
    parser.add_argument('--soc-root',type=Path,default=ROOT.parent/'bitblt_soc-ysl-dev/embedded_sw/newSoC')
    parser.add_argument('--riscv-bin',help='Compiler prefix ending in riscv-none-elf-')
    args=parser.parse_args();gfx=args.gfx_root.resolve();soc=args.soc_root.resolve()
    hardware=soc.parents[1]
    contract=json.loads((gfx/'platform/sapphire/publish_contract.json').read_text(encoding='utf8'))
    expected=contract['files']
    for rel,digest in expected.items():
        p=(hardware/rel).resolve()
        if not p.is_relative_to(hardware) or not p.is_file() or sha(p)!=digest:
            raise SystemExit('Hardware publication contract mismatch: '+str(p))
    prefix=args.riscv_bin or os.environ.get('RISCV_BIN') or str(gfx/'build/toolchains/xpack-riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf-')
    for tool in ['gcc','nm','readelf','objdump']:
        if not Path(prefix+tool).is_file():raise SystemExit('Missing existing toolchain: '+prefix+tool)
    out=ROOT/'build/sapphire'
    if not out.resolve().is_relative_to(ROOT) or any(p.is_symlink() for p in [ROOT/'build',out]):
        raise SystemExit('Build output must stay inside the game workspace')
    out.mkdir(parents=True,exist_ok=True)
    inputs={}
    for folder in [ROOT,gfx]:
        for directory,dirs,files in os.walk(folder):
            dirs[:]=[d for d in dirs if d not in {'build','.git','__pycache__'}]
            for name in files:
                p=Path(directory)/name
                if p.suffix in {'.c','.h','.py','.ld','.mk','.json'} or name=='Makefile':inputs[str(p)]=sha(p)
    report={'hardware_contract':contract,'sources':inputs,'commands':[],'scope':'cross_compile_only_no_board_run'}
    def run(cmd):
        result=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        report['commands'].append({'command':[str(x) for x in cmd],'returncode':result.returncode,'output':result.stdout})
        (out/'build_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
        if result.returncode:raise SystemExit(result.stdout)
        return result.stdout
    report['compiler']=run([prefix+'gcc','--version']).splitlines()[0]
    print('Building against matched CPU/DDR publication contract; no board access.',flush=True)
    output=run(['make','-B','-f','platform/sapphire.mk',f'GFX_ROOT={gfx}',f'SOC_ROOT={soc}',f'RISCV_BIN={prefix}','all'])
    print(output,flush=True)
    elf=out/'ti60_danmaku.elf'
    header=run([prefix+'readelf','-hW',elf]);segments=run([prefix+'readelf','-lW',elf])
    if not all(x in header for x in ['ELF32',"little endian",'RISC-V']):raise SystemExit('Unexpected ELF architecture')
    loads=[]
    for line in segments.splitlines():
        cols=line.split()
        if cols and cols[0]=='LOAD':
            address=int(cols[2],16);physical=int(cols[3],16);size=int(cols[5],16)
            if physical!=address or not 0x1000<=address<address+size<=0x201000:
                raise SystemExit('LOAD escapes the reserved RAM layout: '+line)
            loads.append({'begin':address,'end':address+size,'file_bytes':int(cols[4],16)})
    if not loads:raise SystemExit('No LOAD segments')
    symbols={}
    for line in run([prefix+'nm','-S','--defined-only',elf]).splitlines():
        c=line.split()
        if len(c)==4:symbols[c[3]]={'address':int(c[0],16),'bytes':int(c[1],16),'kind':c[2]}
        elif len(c)==3:symbols[c[2]]={'address':int(c[0],16),'bytes':0,'kind':c[1]}
    owned={'game_dma_storage':252672,'game_cpu_frame':614400,'sapphire_publish_drain':64}
    for name,size in owned.items():
        value=symbols.get(name)
        if not value or value['bytes']!=size or value['address']%64 or value['kind'] not in 'bB':
            raise SystemExit('Owned BSS allocation mismatch: '+name+' '+str(value))
        if not symbols['__bss_start']['address']<=value['address']<value['address']+size<=symbols['_end']['address']:
            raise SystemExit('Owned allocation escapes BSS: '+name)
    asm=run([prefix+'objdump','-d',elf])
    if '0000500f' not in asm:raise SystemExit('Reviewed DataCache invalidation instruction missing')
    # Detect concurrent source edits; never label a mixed build as verified.
    if any(sha(Path(p))!=digest for p,digest in inputs.items()):raise SystemExit('Source changed during build')
    if any(sha(hardware/rel)!=digest for rel,digest in expected.items()):raise SystemExit('BSP/RTL changed during build')
    report.update({'result':'PASS','elf_sha256':sha(elf),'loads':loads,
        'owned_bss':{name:symbols[name] for name in owned},'stack_bytes':symbols['_sp']['address']-symbols['_stack_bottom']['address'],
        'reserved_ram_bytes':0x200000,'static_plus_stack_end':symbols['_sp']['address'],
        'memory_bytes_from_origin':symbols['_sp']['address']-0x1000,
        'workspace_symbols':{k:v for k,v in symbols.items() if k in ['cpu_workspace','g2d_workspace','renderer','resources','game']}})
    (out/'build_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('PASS: RAM ELF, owned DMA allocations, publication instruction, stack and hardware/source identity verified.')
    print('Memory including code/data/BSS/stack:',report['memory_bytes_from_origin'],'of',report['reserved_ram_bytes'],'bytes')
    print('ELF:',elf)
if __name__=='__main__':main()
