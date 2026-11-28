"""DPF full-screen identity, fallback menus, bundle wiring and private settings."""
import ctypes
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

SOURCE=Path(__file__).resolve().parents[1]
FULL=SOURCE/'fullscreen'
spec=importlib.util.spec_from_file_location('dpf_overlay',FULL/'emulator/library_overlay.py')
overlay=importlib.util.module_from_spec(spec);spec.loader.exec_module(overlay)

class FullDpfResources(unittest.TestCase):
    def test_connector_does_not_replace_a_dashboard_or_companion(self):
        identities={}
        for path in (FULL/'native/generated').glob('*.manifest'):
            tree=ET.parse(path);connector=tree.findtext('.//pluginAppConnector/id')
            self.assertNotIn(connector,identities,(path,identities.get(connector)))
            identities[connector]=tree.findtext('name')
        self.assertEqual(identities['243'],'ID6FullDPFDeck')
        self.assertEqual(identities['239'],'ID6FullBootmod3Suite')
        for connector,name in ((231,'BoostDeck'),(234,'EngineCodes'),(238,'WarmDeck'),
                               (240,'MgFlasher'),(241,'XDriveControl'),(242,'HybridDeck')):
            self.assertEqual(identities[str(connector)],'ID6Full'+name)
        self.assertIn('ID6FullDPFDeck surface_dpf',(FULL/'native/generated/modules.txt').read_text())

    def test_settings_and_native_fallback_fit_every_generated_layout(self):
        for folder,right in (('',1260),('layouts/800x480/',720),('layouts/960x480/',900)):
            tree=ET.parse(FULL/('native/generated/'+folder+'ID6FullDPFDeck.xml'))
            main=tree.find(".//hmiState[@id='208']");self.assertIsNotNone(main)
            self.assertIsNotNone(main.find("optionComponents/button[@action='2210']"))
            for model in range(1560,1569):
                self.assertIsNotNone(main.find("components/label[@model='%d']"%model))
                self.assertIsNotNone(tree.find(".//models/raDataModel[@id='%d']"%model))
            settings=tree.find(".//hmiState[@id='2210']")
            for callback in range(5920,5926):
                self.assertIsNotNone(settings.find("components/button[@action='%d']"%(callback+10000)))
                combined=tree.find(".//combinedAction[@id='%d']/actions"%(callback+10000))
                self.assertEqual(combined.find('raAction').get('id'),str(callback))
                self.assertEqual(combined.find('hmiAction').get('target'),'208')
            self.assertIn('unavailable',main.find("components/label[@id='1950']").get('text'))
            for component in tree.findall('.//components/*'):
                values={p.get('id'):int(p.get('value')) for p in component.findall('properties/property')}
                self.assertLessEqual(values.get('20',0)+values.get('9',0),right)
                self.assertLessEqual(values.get('21',0)+values.get('10',0),477)
            for selector in ('.//models/*','.//actions/*','.//hmiStates/*'):
                ids=[node.get('id') for node in tree.findall(selector)]
                self.assertEqual(len(ids),len(set(ids)))

    def test_d1_d2_bridge_validation_and_persistence_are_independent(self):
        compiler=shutil.which('cc');self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'validate.c';library=root/'validate.so'
            source.write_text('#include "boost_config.h"\nint validate(const unsigned char* d,size_t n) { return boost_config_valid(5,d,n); }\n')
            subprocess.run([compiler,'-std=c99','-Wall','-Wextra','-Werror','-fPIC','-shared',
                '-I'+str(FULL/'emulator'),str(source),'-o',str(library)],check=True,capture_output=True)
            native=ctypes.CDLL(str(library));native.validate.argtypes=(ctypes.c_void_p,ctypes.c_size_t)
            instance=overlay.LibraryOverlay.__new__(overlay.LibraryOverlay)
            instance._config=tuple(root/name for name in ('prefs','layout','mode','scale','codes','dpf'))
            for index,path in enumerate(instance._config[:5]): path.write_bytes(bytes((index,)))
            cases=[]
            for bank in (0,1):
                for metric in (0,1):
                    cases.append((bytes((ord('D'),1,bank,metric)),True))
                    for regen in (0,1): cases.append((bytes((ord('D'),2,bank,metric,regen)),True))
            cases.extend(((b'D\x02\0\x01\x02',False),(b'D\x02\x02\x01\0',False),
                          (b'D\x02\0\x02\0',False),(b'D\x03\0\x01\0',False),
                          (b'D\x01\0\x01\0',False),(b'D\x02\0\x01',False),(b'',False)))
            for data,valid in cases:
                self.assertEqual(bool(native.validate(ctypes.create_string_buffer(data),len(data))),valid)
                self.assertEqual(overlay.LibraryOverlay._valid_config(5,data),valid)
                before=instance._config[5].read_bytes() if instance._config[5].exists() else None
                instance._save_config(f'ID6CONF 5 {data.hex()}\n'.encode())
                after=instance._config[5].read_bytes() if instance._config[5].exists() else None
                self.assertEqual(after,data if valid else before)
            for index,path in enumerate(instance._config[:5]): self.assertEqual(path.read_bytes(),bytes((index,)))

if __name__=='__main__': unittest.main()
