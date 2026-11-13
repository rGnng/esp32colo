"""Recovery write boundaries and host evidence required for a concurrent pass."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from test_ota_upload import application

SCRIPTS=Path(__file__).resolve().parents[1]/'scripts'
def module(name):
    spec=importlib.util.spec_from_file_location(name,SCRIPTS/(name+'.py'))
    value=importlib.util.module_from_spec(spec);sys.modules[name]=value;spec.loader.exec_module(value)
    return value
recovery=module('recover_usb')
host=module('check_usb_concurrency')

class RecoveryTests(unittest.TestCase):
    def test_reject_bad_chip_bootloader_integrity_and_trailing_bytes(self):
        image=application();recovery.validate(image)
        for at in (0,12,32,40,len(image)-33,len(image)-1):
            bad=bytearray(image);bad[at]^=1
            with self.subTest(at=at),self.assertRaises(ValueError):recovery.validate(bad)
        for bad in (b'',image[:30],image[:-1],image+b'junk'):
            with self.assertRaises(ValueError):recovery.validate(bad)

    def test_recover_both_slots_without_erase_reset_or_nvs_writes(self):
        commands=recovery.commands('/dev/cu.test',Path('/private/known-good.bin'),460800)
        self.assertEqual(len(commands),2)
        for cmd in commands:
            self.assertEqual(cmd[-4:],['0x20000','/private/known-good.bin','0x4a0000','/private/known-good.bin'])
            self.assertEqual(cmd[cmd.index('--before')+1],'no-reset')
            self.assertEqual(cmd[cmd.index('--after')+1],'no-reset')
            self.assertNotIn('erase-flash',cmd)
        self.assertIn('write-flash',commands[0]);self.assertIn('verify-flash',commands[1])
        for option in ('--flash-mode','--flash-freq','--flash-size'):
            self.assertEqual(commands[0][commands[0].index(option)+1],'keep')

    def test_manifest_mismatch_stops_before_flash_or_host_dependencies(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);image=application()
            (path/'known-good.bin').write_bytes(image)
            (path/'manifest.json').write_text(json.dumps({'sha256':'0'*64,'bytes':len(image)}))
            with patch.object(sys,'argv',['recover_usb.py','--image',str(path/'known-good.bin'),'--port','unused']),patch.object(recovery.subprocess,'run') as run:
                with self.assertRaisesRegex(ValueError,'manifest'):recovery.main()
                run.assert_not_called()
            with self.assertRaisesRegex(RuntimeError,'hash mismatch'):host.preflight(path,None)

class EvidenceTests(unittest.TestCase):
    def setUp(self):
        state={'phase':'coexist-attempt','phy_owner':'usb-otg'}
        self.phase={'device_before':state.copy(),'device_after':state.copy(),**{key:{'ok':True} for key in ('cdc','hid','jtag')}}

    def test_requires_all_host_checks_in_unchanged_otg_phase(self):
        self.assertTrue(host.concurrent_success(self.phase))
        for name in ('cdc','hid','jtag'):
            phase=copy.deepcopy(self.phase);phase[name]={'ok':False}
            self.assertFalse(host.concurrent_success(phase))
            phase.pop(name);self.assertFalse(host.concurrent_success(phase))
        for field,value in (('phase','serial-jtag-baseline'),('phy_owner','usb-serial-jtag')):
            phase=copy.deepcopy(self.phase);phase['device_after'][field]=value
            self.assertFalse(host.concurrent_success(phase))
        self.assertFalse(host.concurrent_success({'otg_stack_started':True,'fixed_serial_driver_kept_initialized':True}))

    def test_jtag_selection_rejects_missing_or_unsafe_serial_without_tools(self):
        with patch.object(host,'tool_run') as run:
            for serial in ('',None,'[shutdown]','a} ; shutdown'):
                self.assertFalse(host.probe_jtag('/unused/openocd',serial)['ok'])
            run.assert_not_called()

if __name__=='__main__':unittest.main()
