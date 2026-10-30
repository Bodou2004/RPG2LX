"""Render actual MV-format map data with SDL dummy; never substitute a Python engine."""
import json
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

BINARY=Path(sys.argv.pop(1)).resolve()
ROOT=Path(__file__).resolve().parents[1]


class NativeGameSmoke(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="rpg2lx playable ")
        self.addCleanup(self.temp.cleanup)
        self.home=Path(self.temp.name)
        self.project=self.home/'games'/'Demo'
        shutil.copytree(ROOT/'fixtures/playable_project',self.project)

    def render(self):
        env=os.environ.copy()
        env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        return subprocess.run([str(BINARY),'launcher','--home',str(self.home),'--start-first',
                               '--smoke-frames','3','--screenshot',str(self.home/'map.bmp')],
                              env=env,capture_output=True,timeout=20)

    def render_source(self,source):
        env=os.environ.copy()
        env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        return subprocess.run([str(BINARY),'run',str(source),'--home',str(self.home),
                               '--smoke-frames','3','--screenshot',str(self.home/'direct.bmp')],
                              env=env,capture_output=True,timeout=20)

    def log(self):
        return (self.home/'logs/launcher.log').read_text(encoding='utf-8',errors='replace')

    def test_village_render_contains_tiles_and_player(self):
        result=self.render()
        self.assertEqual(result.returncode,0,self.log())
        bmp=(self.home/'map.bmp').read_bytes()
        self.assertEqual(bmp[:2],b'BM')
        self.assertEqual(struct.unpack_from('<ii',bmp,18),(640,480))
        pixels=bmp[struct.unpack_from('<I',bmp,10)[0]:]
        self.assertGreater(pixels.count(bytes((77,112,50))),1000, 'Grass PNG must be rendered')
        self.assertGreater(pixels.count(bytes((188,147,70))),10, 'Player sprite must be rendered')

    def test_second_map_boot(self):
        path=self.project/'data/System.json'
        data=json.loads(path.read_text());data.update(startMapId=2,startX=4,startY=7)
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,0,self.log())

    def test_missing_png_has_explicit_error(self):
        (self.project/'img/tilesets/Demo_A2.png').unlink()
        self.assertEqual(self.render().returncode,2)
        self.assertIn('Missing or oversized PNG',self.log())

    def test_encrypted_images_are_rejected(self):
        path=self.project/'data/System.json'
        data=json.loads(path.read_text());data['hasEncryptedImages']=True
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,2)
        self.assertIn('Encrypted images',self.log())

    def test_unknown_autorun_command_stops_with_diagnostic(self):
        path=self.project/'data/Map001.json'
        data=json.loads(path.read_text())
        data['events'][1]['pages'][0]['trigger']=3
        data['events'][1]['pages'][0]['list']=[{'code':355,'indent':0,'parameters':['unavailable()']}]
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,2)
        self.assertIn('Unsupported event command 355',self.log())

    def test_direct_project_does_not_change_source_files(self):
        def snapshot():
            return {str(p.relative_to(self.project)):hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in self.project.rglob('*') if p.is_file()}
        before=snapshot()
        self.assertEqual(self.render_source(self.project).returncode,0,self.log())
        self.assertEqual(snapshot(),before)

    def test_direct_desktop_export_with_www(self):
        export=self.home/'Original export'
        shutil.move(self.project,export/'www')
        entry=export/'Game.exe'
        entry.write_bytes(b'Not executed: resolve the export beside this file')
        self.assertEqual(self.render_source(entry).returncode,0,self.log())
        self.assertFalse((export/'logs').exists())
        self.assertFalse((export/'www/saves').exists())

    def test_direct_mz_is_rejected(self):
        (self.project/'js/rmmz_core.js').write_text('// MZ marker')
        self.assertEqual(self.render_source(self.project).returncode,2)
        self.assertIn('MZ',self.log())


if __name__=='__main__':unittest.main()
