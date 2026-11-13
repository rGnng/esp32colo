#!/usr/bin/env python3
"""Run on the USB host. Measure CDC-ECM + vendor HID + built-in JTAG by phase."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request

OPENER=urllib.request.build_opener(urllib.request.ProxyHandler({}))

def fetch(base,path,data=None):
    raw=None;headers={}
    if data is not None:
        raw=urllib.parse.urlencode(data).encode();headers['Content-Type']='application/x-www-form-urlencoded'
    with OPENER.open(urllib.request.Request(base.rstrip('/')+path,data=raw,headers=headers),timeout=8) as response:
        return json.load(response)

def tool_run(args,timeout=20,limit=6000):
    try:
        result=subprocess.run(args,capture_output=True,text=True,timeout=timeout)
        return {'returncode':result.returncode,'output':(result.stdout+result.stderr)[-limit:]}
    except (OSError,subprocess.TimeoutExpired) as e:return {'returncode':None,'output':str(e)}

def usb_devices():
    records=[]
    if platform.system()=='Darwin':
        try:
            data=subprocess.check_output(['ioreg','-p','IOUSB','-a','-l'],timeout=10)
            def walk(node):
                if isinstance(node,list):
                    for child in node:walk(child)
                elif isinstance(node,dict):
                    if node.get('idVendor')==0x303A:
                        records.append({'vid':node['idVendor'],'pid':node.get('idProduct'),'serial':node.get('USB Serial Number'),'product':node.get('USB Product Name',node.get('IORegistryEntryName'))})
                    for child in node.values():
                        if isinstance(child,(dict,list)):walk(child)
            walk(plistlib.loads(data))
        except Exception as e:return {'devices':[],'error':str(e)}
    elif platform.system()=='Linux':
        for folder in Path('/sys/bus/usb/devices').glob('*'):
            try:
                if int((folder/'idVendor').read_text().strip(),16)!=0x303A:continue
                def read(name):return (folder/name).read_text().strip() if (folder/name).exists() else None
                records.append({'vid':0x303A,'pid':int(read('idProduct'),16),'serial':read('serial'),'product':read('product')})
            except (ValueError,OSError):pass
    else:return {'devices':[],'error':'native USB listing supports macOS/Linux; attach host inventory separately'}
    return {'devices':records}

def serial_ports():
    from serial.tools import list_ports
    return [{'port':p.device,'serial':p.serial_number,'vid':p.vid,'pid':p.pid} for p in list_ports.comports() if p.vid==0x303A]

def openocd_path(explicit=None):
    if explicit:return str(Path(explicit).expanduser().resolve())
    value=shutil.which('openocd')
    if value:return value
    for name in ('openocd','openocd.exe'):
        p=Path.home()/'.platformio/packages/tool-openocd-esp32/bin'/name
        if p.exists():return str(p)
    return None

def probe_jtag(binary,serial):
    if not re.fullmatch(r'[A-Za-z0-9:._-]{1,128}',serial or ''):
        return {'ok':False,'returncode':None,'output':'No safe exact USB serial identifier for target selection'}
    args=[binary]
    root=Path(binary).resolve().parent.parent
    cfg='board/esp32s3-builtin.cfg'
    for folder in (root/'share/openocd/scripts',root/'scripts'):
        if (folder/cfg).exists():args+=['-s',str(folder)];cfg=str(folder/cfg);break
    # Examine the target without reset/flash. Resume if examination halted it.
    args+=['-f',cfg,'-c','adapter serial {'+serial+'}',
           '-c','init; targets; if {[esp32s3.cpu0 curstate] eq "halted"} {resume}; shutdown']
    result=tool_run(args)
    result['ok']=result['returncode']==0 and 'tap/device found' in result['output'] and 'esp32s3.cpu0' in result['output']
    return result

def hid_probe(expected_serial):
    import hid
    devices=[d for d in hid.enumerate(0x303A,0x4003) if d.get('serial_number')==expected_serial]
    if len(devices)!=1:return {'ok':False,'error':'expected one exact test HID device, found '+str(len(devices))}
    dev=hid.device()
    try:
        dev.open_path(devices[0]['path'])
        payload=(b'COLOPLAY-USB-TEST:'+os.urandom(16)).ljust(63,b'\0')
        count=dev.send_feature_report(bytes([6])+payload)
        reply=bytes(dev.get_feature_report(6,64))
        return {'ok':count==64 and reply==bytes([6])+payload,'written':count,'reply_hex':reply.hex(),'expected_hex':(bytes([6])+payload).hex(),'usage_page':devices[0].get('usage_page')}
    except Exception as e:return {'ok':False,'error':str(e)}
    finally:dev.close()

def host_has_ip(address):
    if platform.system()=='Darwin':
        result=tool_run(['ifconfig','-a'],10,limit=200000)
        return bool(re.search(r'\binet\s+'+re.escape(address)+r'\s',result['output']))
    if platform.system()=='Linux':
        result=tool_run(['ip','-j','address'],10,limit=200000)
        try:return any(a.get('local')==address for interface in json.loads(result['output']) for a in interface.get('addr_info',[]))
        except (ValueError,TypeError):return False
    return False

def cdc_probe(control,status):
    usb=status['usb'];expected=usb['dhcp_address']
    deadline=time.monotonic()+20
    while not host_has_ip(expected) and time.monotonic()<deadline:time.sleep(1)
    if not host_has_ip(expected):return {'ok':False,'error':'USB host address not assigned: '+expected}
    try:
        result=fetch('http://'+usb['address'],'/api/status')
        ok=result['sketch_md5']==control['sketch_md5'] and result['wifi']['mac']==control['wifi']['mac']
        return {'ok':ok,'device_address':usb['address'],'host_address':expected,'same_device_image':ok,'usb_rx_frames':result['usb']['rx_frames'],'usb_tx_frames':result['usb']['tx_frames']}
    except Exception as e:return {'ok':False,'error':str(e)}

def wait_phase(url,phase,timeout=25):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        state=fetch(url,'/api/usb-experiment')
        if state['phase']==phase:return state
        if state['phase']=='usb-start-failed':raise RuntimeError('USB stack failed: '+str(state))
        time.sleep(.5)
    raise RuntimeError('phase timeout: '+phase)

def preflight(folder,explicit):
    from recover_usb import validate
    manifest=json.loads((folder/'manifest.json').read_text());image=(folder/'known-good.bin').read_bytes();validate(image)
    if hashlib.sha256(image).hexdigest()!=manifest['sha256'] or len(image)!=manifest['bytes']:raise RuntimeError('local recovery image hash mismatch')
    missing=[name for name in ('hid','serial','esptool') if importlib.util.find_spec(name) is None]
    if missing:raise RuntimeError('missing Python modules: '+', '.join(missing)+'; install requirements-host.txt')
    binary=openocd_path(explicit)
    if not binary or not Path(binary).is_file():raise RuntimeError('Espressif OpenOCD required; install platformio/tool-openocd-esp32 or pass --openocd')
    print('Recovery image verified:',manifest['firmware'],manifest['sha256'],flush=True)
    print('OpenOCD:',binary,flush=True)
    return manifest,binary

def concurrent_success(phase):
    before=phase.get('device_before',{});after=phase.get('device_after',{})
    return (all(phase.get(name,{}).get('ok') is True for name in ('cdc','hid','jtag'))
            and before.get('phase')==after.get('phase')=='coexist-attempt'
            and before.get('phy_owner')==after.get('phy_owner')=='usb-otg')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url',help='WiFi HTTP address, never the USB network address')
    parser.add_argument('--recovery-dir',default=str(Path(__file__).resolve().parent))
    parser.add_argument('--openocd');parser.add_argument('--jtag-serial');parser.add_argument('--preflight',action='store_true')
    parser.add_argument('--output',default='usb-concurrency-report.json')
    args=parser.parse_args();folder=Path(args.recovery_dir).expanduser().resolve()
    manifest,binary=preflight(folder,args.openocd)
    if args.preflight:print('Preflight passed. No device mode changed.');return 0
    url=args.url or manifest.get('control_url')
    if not url:raise RuntimeError('--url must name the device WiFi address')
    initial=fetch(url,'/api/status')
    if 'usb-concurrency-test' not in initial['firmware']:raise RuntimeError('experiment firmware is not running; no USB mode changed')
    if not initial['hardware']['ready'] or initial['recovery_mode']:raise RuntimeError('wait for hardware startup or recover the device first')
    if urllib.parse.urlsplit(url).hostname==initial['usb']['address']:raise RuntimeError('use the WiFi address for control, not the USB IP')
    report={'started_utc':datetime.now(timezone.utc).isoformat(),'control_url':url,'firmware':initial['firmware'],'image':initial['sketch_md5'],'phases':[],'complete':False,'all_three_same_phase':None}
    armed=False
    try:
        if fetch(url,'/api/usb-experiment')['phase']!='serial-jtag-baseline':
            fetch(url,'/api/usb-experiment',{'mode':'jtag'});wait_phase(url,'jtag-with-otg-stack-retained');time.sleep(3)
        ports=[p for p in serial_ports() if p['pid']==0x1001]
        serial=args.jtag_serial
        if not serial:
            if len(ports)!=1:raise RuntimeError('expected one fixed USB Serial/JTAG port; use --jtag-serial to select exactly one device')
            serial=ports[0]['serial']
            canonical=lambda value:re.sub(r'[^a-f0-9]','',str(value).lower())
            if canonical(serial)!=canonical(initial['wifi']['mac']):
                raise RuntimeError('USB serial does not match gadget WiFi MAC; confirm the target and use --jtag-serial explicitly')
        print('Phase 1: fixed Serial/JTAG baseline',flush=True)
        baseline={'name':'serial-jtag-baseline','usb':usb_devices(),'serial_ports':ports,'device':fetch(url,'/api/usb-experiment'),'jtag':probe_jtag(binary,serial)}
        report['phases'].append(baseline)
        if not baseline['jtag']['ok']:raise RuntimeError('baseline JTAG probe failed; experiment was not armed. See OpenOCD output in report.')
        print('Phase 2: CDC-ECM + HID, fixed Serial/JTAG driver kept initialized',flush=True)
        armed=True;fetch(url,'/api/usb-experiment',{'mode':'coexist'})
        before=wait_phase(url,'coexist-attempt');time.sleep(3)
        native=usb_devices();state=fetch(url,'/api/status')
        phase={'name':'coexist-attempt','device_before':before,'usb':native,'serial_ports':serial_ports()}
        phase['hid']=hid_probe(fetch(url,'/api/usb-experiment').get('otg_serial',''))
        phase['cdc']=cdc_probe(initial,state) if before['phy_owner']=='usb-otg' else {'ok':False,'error':'internal PHY is not routed to USB-OTG'}
        phase['jtag']=probe_jtag(binary,serial)
        phase['device_after']=fetch(url,'/api/usb-experiment');report['phases'].append(phase)
        print('  CDC:',phase['cdc']['ok'],'HID echo:',phase['hid']['ok'],'JTAG:',phase['jtag']['ok'],flush=True)
        print('Phase 3: select fixed Serial/JTAG while retaining the OTG stack',flush=True)
        fetch(url,'/api/usb-experiment',{'mode':'jtag'});wait_phase(url,'jtag-with-otg-stack-retained');time.sleep(3)
        third={'name':'jtag-with-otg-stack-retained','device':fetch(url,'/api/usb-experiment'),'usb':usb_devices(),'serial_ports':serial_ports(),'jtag':probe_jtag(binary,serial)}
        third['hid']=hid_probe(before.get('otg_serial',''))
        third['cdc']={'ok':False,'not_probed':'no OTG PHY route; USB inventory recorded rather than sending traffic via an unrelated default route'}
        report['phases'].append(third)
        report['complete']=True
        report['all_three_same_phase']=concurrent_success(phase)
    except Exception as e:report['error']=str(e);print('Check stopped:',e,flush=True)
    finally:
        if armed:
            try:fetch(url,'/api/usb-experiment',{'mode':'jtag'});wait_phase(url,'jtag-with-otg-stack-retained')
            except Exception as e:report['restore_error']=str(e)
        report['finished_utc']=datetime.now(timezone.utc).isoformat()
        path=Path(args.output);path.write_text(json.dumps(report,indent=2)+'\n');os.chmod(path,0o600)
        try:
            raw=json.dumps(report).encode()
            req=urllib.request.Request(url.rstrip('/')+'/api/usb-experiment/report',data=raw,headers={'Content-Type':'application/json'})
            with OPENER.open(req,timeout=8) as response:json.load(response)
            print('Report copied to the gadget for review.',flush=True)
        except Exception as e:print('Could not copy report to gadget:',e,flush=True)
    print('Report:',str(path.resolve()),flush=True)
    print('Complete:',report['complete'],'all three in the same phase:',report['all_three_same_phase'],flush=True)
    return 0 if report['complete'] else 1

if __name__=='__main__':
    try:sys.exit(main())
    except Exception as e:print('error:',e,file=sys.stderr);sys.exit(1)
