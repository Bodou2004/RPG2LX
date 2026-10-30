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
        # The old diagnostic HUD covered the entire top band in this color.
        self.assertNotIn(bytes((34,21,14,255)),pixels[:640*20*4], 'No RPG2LX HUD over gameplay')

    def test_second_map_boot(self):
        path=self.project/'data/System.json'
        data=json.loads(path.read_text());data.update(startMapId=2,startX=4,startY=7)
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,0,self.log())

    def test_missing_png_has_explicit_error(self):
        (self.project/'img/tilesets/Demo_A2.png').unlink()
        self.assertEqual(self.render().returncode,2)
        self.assertIn('Missing or oversized',self.log())

    def test_encrypted_images_are_supported(self):
        png_path=self.project/'img/tilesets/Demo_A2.png'
        raw=bytearray(png_path.read_bytes())
        key_hex='0102030405060708090a0b0c0d0e0f10'
        key=bytes.fromhex(key_hex)
        for i in range(16):
            raw[i]^=key[i]
        rpgmvp_header=bytes([0x52,0x50,0x47,0x4D,0x56,0x00,0x00,0x00,0x00,0x03,0x01,0x00,0x00,0x00,0x00,0x00])
        (self.project/'img/tilesets/Demo_A2.rpgmvp').write_bytes(rpgmvp_header+bytes(raw))
        png_path.unlink()
        path=self.project/'data/System.json'
        data=json.loads(path.read_text())
        data['hasEncryptedImages']=True
        data['encryptionKey']=key_hex
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,0,self.log())

    def test_autorun_js_script_command_is_supported(self):
        path=self.project/'data/Map001.json'
        data=json.loads(path.read_text())
        data['events'][1]['pages'][0]['trigger']=3
        data['events'][1]['pages'][0]['list']=[
            {'code':355,'indent':0,'parameters':['$gameVariables.setValue(1, 42);']},
            {'code':123,'indent':0,'parameters':['A',0]},
            {'code':0,'indent':0,'parameters':[]},
        ]
        data['events'][1]['pages'].append({
            'conditions':{'actorId':1,'actorValid':False,'itemId':1,'itemValid':False,
                          'selfSwitchCh':'A','selfSwitchValid':True,'switch1Id':1,'switch1Valid':False,
                          'switch2Id':1,'switch2Valid':False,'variableId':1,'variableValid':False,'variableValue':0},
            'directionFix':False,'image':{'characterIndex':0,'characterName':'','direction':2,'pattern':0,'tileId':0},
            'list':[{'code':0,'indent':0,'parameters':[]}],
            'moveFrequency':3,'moveRoute':{'list':[{'code':0,'parameters':[]}],'repeat':True,'skippable':False,'wait':False},
            'moveSpeed':3,'moveType':0,'priorityType':0,'stepAnime':False,'through':False,'trigger':0,'walkAnime':True,
        })
        path.write_text(json.dumps(data))
        self.assertEqual(self.render().returncode,0,self.log())

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
