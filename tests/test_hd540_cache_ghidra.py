"""PPC64 ABI proof for our own wrapper using Ghidra; no Sony binary input."""
from pathlib import Path
import os
import struct
import warnings

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'local_only/cache-emulator'
for part in ('projects','tmp','settings'):
    (WORK/part).mkdir(parents=True,exist_ok=True)
for key in ('TEMP','TMP','TMPDIR','APPDATA','LOCALAPPDATA'):
    os.environ[key]=str(WORK/('tmp' if key in ('TEMP','TMP','TMPDIR') else 'settings'))
os.environ['JAVA_HOME']='C:/Program Files/Microsoft/jdk-21.0.12.101-hotspot'
os.environ['GHIDRA_INSTALL_DIR']='C:/ghidra_12.1.3_PUBLIC'
warnings.filterwarnings('ignore',category=DeprecationWarning)
import pyghidra
from pyghidra.launcher import HeadlessPyGhidraLauncher
launch=HeadlessPyGhidraLauncher();launch.java_home=os.environ['JAVA_HOME']
launch.add_vmargs('-Xmx2500m','-Duser.home='+str(WORK/'settings'),'-Dapplication.settingsdir='+str(WORK/'settings'))
launch.start()
from ghidra.app.emulator import EmulatorHelper
from ghidra.util.task import ConsoleTaskMonitor
from java.math import BigInteger

CODE,NATIVE,TRACE,STACK,RETURN,FP=0x8003f800,0x8000d8ec,0xc000de00,0x10008000,0x20000000,0xa000a000
PCRTC=0xa0006000
data=(ROOT/'build/hd540/cache/cache.bin').read_bytes()
with pyghidra.open_program(str(ROOT/'build/hd540/cache/cache.bin'),project_location=str(WORK/'projects'),
                         project_name='self_authored_cache_wrapper',analyze=False,
                         language='PowerPC:BE:64:default') as flat:
    program=flat.getCurrentProgram();space=program.getAddressFactory().getDefaultAddressSpace()
    def address(n):return space.getAddress(format(n,'x'))
    # All simulated memory is owned by this test. Avoid relying on raw importer base.
    for at,size in [(0x8003f000,0x1000),(0x8000d000,0x1000),(0xc000b000,0x1000),
                    (0xc000d000,0x1000),(0xa0001000,0x1000),(FP,0x1000),(PCRTC,0x1000),
                    (0x10000000,0x10000),(RETURN,0x1000)]:
        if program.getMemory().getBlock(address(at)) is None:
            flat.createMemoryBlock('test_'+hex(at),address(at),bytes(size),False)
    for status in (0,6,0x1234567800000000):
        emu=EmulatorHelper(program);emu.enableMemoryWriteTracking(True)
        def wr(at,b):emu.writeMemory(address(at),b)
        def read(at,n):return bytes(emu.readMemory(address(at),n))
        def reg(name):return int(str(emu.readRegister(name)))
        def setreg(name,value):emu.writeRegister(name,BigInteger(str(value)))
        def pc(at):emu.getEmulator().setExecuteAddress(at)
        wr(CODE,data);wr(NATIVE,bytes.fromhex('4e800020'))
        wr(0xc000b440,struct.pack('>Q',0xc000d4c8));wr(0xc000d4c8,struct.pack('>Q',0xa0001860))
        wr(0xa0001880,struct.pack('>Q',0x9f98a000));wr(TRACE,bytes(48))
        wr(0xa0001878,struct.pack('>Q',0x9fa06000))
        wr(PCRTC+0x820,struct.pack('>I',0x80004000));wr(PCRTC+0x210,struct.pack('>I',1079))
        expected=[0x11003011,0x1010,0,539]
        for offset,value in zip([0x848,0x880,0x814,0x818],expected):wr(FP+offset,struct.pack('>I',value))
        wr(FP+0x89c,struct.pack('>I',0x20000000))
        setreg('r1',STACK);setreg('r2',0xc0012f40);setreg('LR',RETURN)
        args={i:0x100000000+i for i in range(3,11)}
        saved={i:0x1234000000000000+i for i in range(14,32)}
        for i,value in (args|saved).items():setreg('r'+str(i),value)
        pc(CODE);calls=0;cache=[]
        for step in range(500):
            at=emu.getExecutionAddress().getOffset()
            if at==RETURN:break
            if at==NATIVE:
                assert reg('r1')==STACK-144 and reg('r2')==0xc0012f40
                assert all(reg('r'+str(i))==v for i,v in args.items())
                assert int.from_bytes(read(TRACE+4,4),'big')==1
                link=reg('LR');wr(STACK-144+16,struct.pack('>Q',0xdeadbeef))
                for i in [0,4,5,6,7,8,9,10,11,12]:setreg('r'+str(i),0xbad00000+i)
                setreg('r3',status);pc(link);calls+=1;continue
            word=int.from_bytes(read(at,4),'big');opcode,xo=word>>26,(word>>1)&1023
            if opcode==31 and xo in (54,982):
                cache.append(('dcbst' if xo==54 else 'icbi',reg('r11')));pc(at+4);continue
            if word in (0x7c0004ac,0x4c00012c):
                cache.append(('sync' if word==0x7c0004ac else 'isync',0));pc(at+4);continue
            assert emu.step(ConsoleTaskMonitor()),(hex(at),emu.getLastError())
        else:raise AssertionError('Wrapper did not return')
        assert calls==1 and reg('r1')==STACK and reg('r2')==0xc0012f40 and reg('r3')==status
        assert all(reg('r'+str(i))==v for i,v in saved.items())
        trace=struct.unpack('>12I',read(TRACE,48))
        assert trace[:2]==(1,0) and list(trace[2:6])==expected and trace[6]==(status&0xffffffff)
        assert trace[7]==0x20000000
        assert trace[8:]==(0x80004000,1079,0,0)
        assert cache==([('dcbst',a) for a in (0x80002f48,0x80003058,0x80003100,0x800036c8)]+[('sync',0)]+
                       [('icbi',a) for a in (0x800036c8,0x80003100,0x80003058,0x80002f48)]+[('sync',0),('isync',0),('sync',0)])
        writes=emu.getTrackedMemoryWriteSet()
        assert not writes.intersects(address(FP),address(FP+0xfff))
        assert not writes.intersects(address(PCRTC),address(PCRTC+0xfff))
        emu.dispose()
    # Emulate the new, self-authored pitch instruction, rather than a C
    # implementation of its behavior. Every accepted aligned source pitch
    # must double exactly and keep the original instruction's low-zero bits.
    emu=EmulatorHelper(program)
    word=(21<<26)|(5<<21)|(5<<16)|(1<<11)|(28<<1)
    for value in (7680,8192,12288,16384,32760):
        emu.writeMemory(address(CODE),struct.pack('>I',word))
        emu.writeRegister('r5',BigInteger(str(value)))
        emu.getEmulator().setExecuteAddress(CODE)
        assert emu.step(ConsoleTaskMonitor())
        assert int(str(emu.readRegister('r5')))==value*2
    emu.dispose()
    print('PASS: Ghidra PPC64 emulation preserves arguments/stack/TOC/callee-saved registers/64-bit return, exact cache operations and read-only register telemetry',flush=True)
