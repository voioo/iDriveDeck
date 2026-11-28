"""Full-screen fault menu contracts and private emulator profile persistence."""
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
spec=importlib.util.spec_from_file_location('fault_overlay',FULL/'emulator/library_overlay.py')
overlay=importlib.util.module_from_spec(spec);spec.loader.exec_module(overlay)


class FullFaultResources(unittest.TestCase):
    def test_read_categories_profiles_confirmation_and_fallback_are_bound_on_every_layout(self):
        for folder,right in (('',1260),('layouts/800x480/',720),('layouts/960x480/',900)):
            tree=ET.parse(FULL/('native/generated/'+folder+'ID6FullEngineCodes.xml'))
            main=tree.find(".//hmiState[@id='203']")
            self.assertIsNotNone(main.find("components/button[@action='5890']"))
            self.assertIsNotNone(main.find("optionComponents/button[@action='2200']"))
            settings=tree.find(".//hmiState[@id='2200']")
            self.assertIsNotNone(settings)
            for callback in (5880,5881,5893,5897,5900):
                self.assertIsNotNone(settings.find("components/button[@action='%d']"%(callback+10000)))
                combined=tree.find(".//actions/combinedAction[@id='%d']/actions"%(callback+10000))
                self.assertEqual(combined.find('raAction').get('id'),str(callback))
                self.assertEqual(combined.find('hmiAction').get('target'),'203')
            confirm=tree.find(".//hmiState[@id='217']")
            self.assertIn('BMW',confirm.get('text'))
            self.assertIn('Permanent',confirm.find("components/label[@id='528']").get('text'))
            self.assertEqual(tree.find(".//combinedAction[@id='233']/actions/raAction").get('id'),'234')
            self.assertEqual(tree.find(".//actions/hmiAction[@id='236']").get('target'),'203')
            for model in (*range(1460,1464),*range(1870,1874)):
                self.assertIsNotNone(tree.find(".//models/raDataModel[@id='%d']"%model))
                self.assertIsNotNone(main.find("components/label[@model='%d']"%model))
            for node in tree.findall('.//components/*'):
                values={p.get('id'):int(p.get('value')) for p in node.findall('properties/property')}
                self.assertLessEqual(values.get('20',0)+values.get('9',0),right)
                self.assertLessEqual(values.get('21',0)+values.get('10',0),477)
            for selector in ('.//models/*','.//actions/*','.//hmiStates/*'):
                ids=[node.get('id') for node in tree.findall(selector)]
                self.assertEqual(len(ids),len(set(ids)))

    def test_e1_has_matching_c_and_python_validation_and_only_its_own_persistence(self):
        compiler=shutil.which('cc');self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'validate.c';library=root/'validate.so'
            source.write_text('#include "boost_config.h"\nint validate(const unsigned char* d,size_t n) { return boost_config_valid(4,d,n); }\n')
            subprocess.run([compiler,'-std=c99','-Wall','-Wextra','-Werror','-fPIC','-shared',
                            '-I'+str(FULL/'emulator'),str(source),'-o',str(library)],check=True,capture_output=True)
            native=ctypes.CDLL(str(library));native.validate.argtypes=(ctypes.c_void_p,ctypes.c_size_t)
            instance=overlay.LibraryOverlay.__new__(overlay.LibraryOverlay)
            instance._config=tuple(root/name for name in ('prefs','layout','mode','scale','codes'))
            for index,path in enumerate(instance._config[:4]): path.write_bytes(bytes((index,)))
            cases=((b'E\x01\0\0',True),(b'E\x01\x01\0',True),(b'E\x01\x02\0',False),
                   (b'E\x02\0\0',False),(b'E\x01\0\x01',False),(b'E\x01\0',False),
                   (b'E\x01\0\0\0',False),(b'',False))
            for data,valid in cases:
                self.assertEqual(bool(native.validate(ctypes.create_string_buffer(data),len(data))),valid)
                self.assertEqual(overlay.LibraryOverlay._valid_config(4,data),valid)
                before=instance._config[4].read_bytes() if instance._config[4].exists() else None
                instance._save_config(f'ID6CONF 4 {data.hex()}\n'.encode())
                after=instance._config[4].read_bytes() if instance._config[4].exists() else None
                self.assertEqual(after,data if valid else before)
            for index,path in enumerate(instance._config[:4]): self.assertEqual(path.read_bytes(),bytes((index,)))


if __name__=='__main__': unittest.main()
