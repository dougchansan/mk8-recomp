from pathlib import Path
import subprocess,struct,tempfile,sys,os
exe=Path(sys.argv[1]).resolve();flags=subprocess.CREATE_NO_WINDOW|subprocess.NORMAL_PRIORITY_CLASS if os.name=='nt' else 0
with tempfile.TemporaryDirectory(prefix='raw-compute-test-') as name:
    root=Path(name);code=root/'authored-code.bin';code.write_bytes(struct.pack('<QQ',0,0xe30000000007000f));control=bytearray(0x758)
    def put(data,offset,value):struct.pack_into('<I',data,offset,value)
    for offset,value in [(0,0x98761234),(0x6f8,16),(0x700,16),(0x704,16),(0x748,32),(0x74c,1),(0x750,1)]:put(control,offset,value)
    checks=0
    def run(label,data,success):
        global checks
        ctrl=root/(label+'.ctrl');out=root/(label+'.spv');ctrl.write_bytes(data);p=subprocess.run([str(exe),str(code),str(ctrl),str(out)],capture_output=True,text=True,timeout=30,creationflags=flags)
        assert (p.returncode==0)==success,(label,p.stdout,p.stderr)
        if success:assert out.read_bytes()[:4]==struct.pack('<I',0x07230203)
        else:assert not out.exists(),label
        checks+=1
    run('raw-compute',control,True)
    run('truncated-control',control[:0x757],False)
    for label,offset,value in [('zero-group',0x748,0),('large-group',0x748,1025),('bad-shared',0x754,3),('bad-start',0x708,0x30),('bad-extent',0x6f8,24),('bad-constant',0x6fc,4)]:
        bad=bytearray(control);put(bad,offset,value);run(label,bad,False)
print(f'raw compute authored cases={checks} PASS')
