"""Automated timed native PC-mode investigation with real OBS evidence.

Requires the separately built/verified crt_display_probe already resident.
Never installs, writes console memory, edits boot settings or evicts a plugin.
The helper owns automatic restoration even if this client disconnects.
"""
import argparse
from datetime import datetime, timezone
import ftplib
import hashlib
import io
import json
from pathlib import Path
import secrets
import struct
import subprocess
import sys
import time
from urllib.request import urlopen
from hd540_auto_test import Capture, parse_slots

ROOT=Path(__file__).resolve().parents[1]
MAGIC=0x44535032
BASE="/dev_hdd0/tmp/crt_display_probe"
REQUEST,REPLY,READY=BASE+".request",BASE+".reply",BASE+".v2.ready"
ACTIONS={"inspect":1,"stock":2,"caps":3,"wxga":4,"sxga":4,"wuxga":4}
TERMINAL={5,6,7}


class DisplayConsole:
    def __init__(self,host,folder):
        self.host,self.folder=host,folder
        self.ftp=ftplib.FTP(host,timeout=6);self.ftp.login();self.pending=None

    def read(self,path,optional=False):
        for attempt in range(2):
            out=io.BytesIO()
            try:
                self.ftp.retrbinary("RETR "+path,out.write)
                return out.getvalue()
            except ftplib.error_perm as error:
                if optional and str(error).startswith("550"):return None
                raise
            except (OSError,EOFError,ftplib.error_temp):
                if attempt:raise
                # Retry only a read, never STOR/rename or an uncertain command.
                self.ftp.close()
                self.ftp=ftplib.FTP(self.host,timeout=6);self.ftp.login()

    def reply(self):
        data=self.read(REPLY,True)
        if not data or len(data)!=16:return None
        magic,sequence,state,detail=struct.unpack(">4I",data)
        if magic!=MAGIC or state>8:raise RuntimeError("Unknown display-probe ABI")
        return dict(sequence=sequence,state=state,detail=detail)

    def preflight(self):
        for path in ("/dev_hdd0/tmp/240p_app.active","/dev_hdd0/tmp/240p_enforcer.on"):
            if self.read(path,True) is not None:raise RuntimeError("App/enforcement must be paused: "+path)
        reply=self.reply()
        if reply and (reply["state"] not in TERMINAL or reply["state"]==7):
            raise RuntimeError("Probe busy or recovery latched; no new test")
        page=urlopen(f"http://{self.host}/vshplugin.ps3mapi",timeout=6).read().decode(errors="replace")
        slots=parse_slots(page)
        resident=[v for v in slots.values() if v["name"]=="crt_display_probe"]
        if resident!=[{"name":"crt_display_probe","path":BASE+".sprx"}] or self.read(READY,True) is None:
            raise RuntimeError("Matching live probe is not ready")
        local=ROOT/"build/display-probe/plugin/vsh_240p.sprx"
        if self.read(BASE+".sprx")!=local.read_bytes():raise RuntimeError("Probe file identity mismatch")

    def publish(self,action,cancel=False,profile=1):
        if not cancel:self.preflight()
        old=self.read(REQUEST,True);reply=self.reply()
        if old and not cancel:
            if len(old)!=16 or not reply or struct.unpack(">4I",old)[:2]!=(MAGIC,reply["sequence"]):
                raise RuntimeError("Previous inbox unacknowledged; no replacement")
        sequence=secrets.randbelow(0xfffffffe)+1
        if old and len(old)==16 and sequence==struct.unpack(">4I",old)[1]:sequence=sequence%0xffffffff+1
        data=struct.pack(">4I",MAGIC,sequence,action,profile)
        stage=f"{REQUEST}.{sequence}.stage";backup=f"{REQUEST}.{sequence}.prev"
        if self.read(stage,True) is not None or self.read(backup,True) is not None:raise RuntimeError("Staging conflict")
        self.ftp.storbinary("STOR "+stage,io.BytesIO(data))
        if self.read(stage)!=data:raise RuntimeError("Request staging readback failed")
        if self.read(REQUEST,True)!=old:raise RuntimeError("Inbox changed during staging")
        if old is not None:
            (self.folder/f"inbox-before-{sequence}.bin").write_bytes(old)
            self.ftp.rename(REQUEST,backup)
        self.pending=sequence
        try:self.ftp.rename(stage,REQUEST)
        except ftplib.error_perm:
            if old is not None:self.ftp.rename(backup,REQUEST)
            self.pending=None;raise
        if self.read(REQUEST)!=data:raise RuntimeError("Published request readback failed")
        return sequence

    def cancel_pending(self):
        if self.pending is None:return
        reply=self.reply()
        if reply and reply["sequence"]==self.pending and reply["state"] in TERMINAL:
            if reply["state"]==7:raise RuntimeError("Recovery latched; no further requests")
            self.pending=None;return
        sequence=self.publish(5,cancel=True)
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            reply=self.reply()
            if reply and reply["sequence"]==sequence and reply["state"] in TERMINAL:
                if reply["state"]!=5:raise RuntimeError("Cancelled test did not confirm restoration")
                self.pending=None;return
            time.sleep(.25)
        raise RuntimeError("Cancellation recovery unconfirmed; no further tests")

    def logs(self,name):
        for remote,suffix in ((BASE+".log","log"),(REPLY,"reply.bin")):
            data=self.read(remote,True)
            if data is not None:(self.folder/f"{name}.{suffix}").write_bytes(data)

    def close(self):self.ftp.close()


def receiver(scan=False,timing=False):
    command=[sys.executable,"-B",str(ROOT/"tools/capture_input_status.py"),"--json"]
    if timing:command.append("--timing-once")
    elif scan:command.append("--scan-once")
    result=subprocess.run(command,capture_output=True,text=True,encoding="utf-8",timeout=6)
    if result.returncode:raise RuntimeError("Receiver status unavailable: "+result.stderr.strip())
    return json.loads(result.stdout)


def run_test(console,capture,name,folder,baseline,scan,selected="wxga",timing=False):
    profile={"sxga":2,"wuxga":3}.get(selected if name=="caps" else name,1)
    sequence=console.publish(ACTIONS[name],profile=profile);print(f"{name}: sequence {sequence}",flush=True)
    result={"name":name,"sequence":sequence,"frames":[],"refresh_hz":None,"geometry_review":"pending"}
    start=time.monotonic();active_start=None;last_state=None;stamp=None
    while time.monotonic()-start<55:
        reply=console.reply()
        if reply and reply["sequence"]==sequence:
            state=reply["state"];elapsed=time.monotonic()-start
            if state!=last_state:print(f"{name}: state {state}, detail {reply['detail']}",flush=True);last_state=state
            stats,new_stamp=capture.frame(folder/f"{name}-{len(result['frames']):03d}-state{state}.jpg")
            if new_stamp!=stamp:
                result["frames"].append(dict(elapsed=round(elapsed,3),state=state,**stats));stamp=new_stamp
            if state==3:
                if active_start is None:active_start=time.monotonic()
                if "receiver" not in result and time.monotonic()-active_start>=7:
                    result["receiver"]=receiver(scan and name in ("wxga","sxga","wuxga"),timing and name in ("wxga","sxga","wuxga"))
                    result["live_scan_query_may_disturb_capture"]=bool((scan or timing) and name in ("wxga","sxga","wuxga"))
            if state in TERMINAL:
                result.update(reply)
                if state!=7:console.pending=None
                break
        time.sleep(.25)
    else:raise RuntimeError(name+": timeout; cancellation required")
    recovered=[]
    for i in range(8):
        time.sleep(.5);stats,_=capture.frame(folder/f"{name}-recovery-{i}.jpg");recovered.append(stats["picture_detected"])
    result["restored_receiver"]=receiver()
    result["recovery_picture_detected"]=sum(recovered)>=6
    active=[r for r in result["frames"] if r["state"]==3]
    mature=[r for r in active if r["elapsed"]>=active[0]["elapsed"]+4] if active else []
    result["active_picture_fraction"]=sum(r["picture_detected"] for r in mature)/len(mature) if mature else 0
    expected=(1280,720) if name=="stock" else {"wxga":(1280,768),"sxga":(1280,1024),"wuxga":(1920,1200)}[name] if name in ("wxga","sxga","wuxga") else (baseline["reported_width"],baseline["reported_height"])
    actual=result.get("receiver",{})
    result["receiver_matches"]=name=="inspect" or (actual.get("reported_width"),actual.get("reported_height"))==expected and actual.get("reported_signal_present") is True
    if timing and name in ("wxga","sxga","wuxga"):
        result["receiver_matches"]=(actual.get("live_reported_width"),actual.get("live_reported_height"))==expected and actual.get("live_reported_signal_present") is True
    returned=result["restored_receiver"]
    result["receiver_restored"]=(returned["reported_width"],returned["reported_height"] )==(baseline["reported_width"],baseline["reported_height"]) and returned["reported_signal_present"]
    result["passed"]=bool(result["state"]==5 and result["detail"]==0 and result["recovery_picture_detected"] and result["receiver_restored"] and result["receiver_matches"] and (name=="inspect" or len(mature)>=3 and result["active_picture_fraction"]>=.8))
    console.logs(name);(folder/f"{name}.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(f"{name}: {'PASS' if result['passed'] else 'FAIL'}; target={expected}, receiver={actual}, returned={returned}",flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host",default="192.168.0.169")
    parser.add_argument("--action",choices=[*ACTIONS,"all"],default="inspect")
    parser.add_argument("--profile",choices=["wxga","sxga","wuxga"],default="wxga",help="Target for caps/all")
    parser.add_argument("--timing-candidate",action="store_true",help="Separate diagnostic: one potentially disruptive live input dimension/scan sample")
    parser.add_argument("--scan-candidate",action="store_true",help="One possibly disruptive live scan query during WXGA; separate from undisturbed validation")
    args=parser.parse_args();folder=ROOT/"local_only/display_presets/automatic"/datetime.now().strftime("%Y%m%d-%H%M%S")
    folder.mkdir(parents=True);report={"time":datetime.now(timezone.utc).isoformat(),"tests":[],"status":"starting"}
    console=capture=None
    try:
        console=DisplayConsole(args.host,folder);console.preflight();capture=Capture(folder);capture.start()
        stats,_=capture.frame(folder/"baseline.jpg");report["baseline_picture"]=stats
        if not stats["picture_detected"]:raise RuntimeError("Baseline picture absent/flat")
        baseline=receiver();report["baseline_receiver"]=baseline
        if not baseline["reported_signal_present"]:raise RuntimeError("Baseline input signal absent")
        actions=["inspect","stock","caps",args.profile] if args.action=="all" else [args.action]
        for name in actions:
            result=run_test(console,capture,name,folder,baseline,args.scan_candidate,args.profile,args.timing_candidate);report["tests"].append(result)
            if not result["passed"]:raise RuntimeError(name+" failed; sequence stopped after recovery")
        report["status"]="passed_helper_picture_and_receiver_checks"
    except (Exception,KeyboardInterrupt) as error:
        report.update(status="stopped",error=str(error) or "interrupted");print("STOP:",report["error"],flush=True)
    finally:
        if console:
            try:console.cancel_pending()
            except Exception as error:report.update(status="recovery_unconfirmed",recovery_error=str(error))
            try:console.logs("final")
            except Exception as error:report["log_error"]=str(error)
            console.close()
        if capture:capture.close()
        (folder/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8");print("Evidence:",folder,flush=True)
    return 0 if report["status"]=="passed_helper_picture_and_receiver_checks" else 1


if __name__=="__main__":raise SystemExit(main())
