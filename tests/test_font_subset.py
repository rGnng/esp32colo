"""Coverage policy and anonymous library supplement generation."""
import importlib.util
from pathlib import Path
import tempfile
import struct
import unittest
spec=importlib.util.spec_from_file_location('subset',Path(__file__).resolve().parents[1]/'scripts/playback_font_subset.py')
subset=importlib.util.module_from_spec(spec);spec.loader.exec_module(subset)
class SubsetTests(unittest.TestCase):
    def test_baseline_keeps_japanese_and_latin(self):
        for char in 'AZaz09?日本語ひらがなカタカナ髙﨑é★':self.assertTrue(subset.common(ord(char)),char)
        self.assertFalse(subset.common(ord('𠮷')))
        self.assertFalse(subset.common(0xd800))
    def test_supplements_keep_observed_outside_baseline(self):
        source={ord('?'),ord('日'),ord('𠮷'),ord('🔊')}
        self.assertEqual(subset.select(source,{ord('𠮷')}),sorted([ord('?'),ord('日'),ord('𠮷')]))
        with self.assertRaises(ValueError):subset.select(source,{0x10ffff})
    def test_scalar_file_roundtrip_and_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'extra.txt';subset.write_extra({0x20bb7,0x4e00},p)
            self.assertEqual(subset.read_extra(p),{0x20bb7,0x4e00})
            self.assertNotIn('track title',p.read_text())
            for bad in ['D800','U+D800','U+110000','U+XYZ']:
                p.write_text(bad+'\n')
                with self.assertRaises(ValueError):subset.read_extra(p)
    def test_only_text_metadata_contributes(self):
        snapshot={'views':{'songs':{'items':[dict(id='PRIVATE-ID',title='日本',artist='é',album='𠮷',track=3)]},'artists':{'items':[dict(name='語')]}}}
        self.assertEqual(set(subset.library_characters(snapshot)),set(map(ord,'日本é𠮷語')))
    def test_chinese_ranking_and_korean_coverage(self):
        chinese=subset.read_extra(subset.CHINESE_FILE)
        self.assertEqual(len(chinese),1500)
        self.assertTrue(all(0x4e00<=p<=0x9fff and chr(p).encode('gb2312') for p in chinese))
        korean=subset.korean_points()
        self.assertEqual(sum(0xac00<=p<=0xd7a3 for p in korean),2350)
        self.assertFalse(any(0x4e00<=p<=0x9fff for p in korean))
        self.assertTrue(set(map(ord,'한글나는각ㅇ'))<=korean)
    def test_shared_characters_always_prefer_japanese(self):
        cmaps={'jp':set(map(ord,'日风한')),'sc':set(map(ord,'日风华')),'kr':set(map(ord,'日한글'))}
        for ch in '日风한':self.assertEqual(subset.glyph_source(ord(ch),cmaps),'jp')
        self.assertEqual(subset.glyph_source(ord('华'),cmaps),'sc')
        self.assertEqual(subset.glyph_source(ord('글'),cmaps),'kr')
        with self.assertRaises(ValueError):subset.glyph_source(ord('🧀'),cmaps)
    def test_hangul_composition(self):
        for cp in range(0xac00,0xd7a4):
            index=cp-0xac00;tail=index%28
            decomposed=chr(0x1100+index//588)+chr(0x1161+index%588//28)+(chr(0x11a7+tail) if tail else '')
            self.assertEqual(subset.compose_hangul(decomposed),chr(cp))
            self.assertEqual(subset.compose_hangul(chr(cp-tail)+(chr(0x11a7+tail) if tail else '')),chr(cp))
        self.assertEqual(subset.compose_hangul('e\u0301ᄀAᅡᆨ'), 'e\u0301ᄀAᅡᆨ')
    def test_generated_font_contains_repertoires(self):
        header=(Path(__file__).resolve().parents[1]/'firmware/include/playback_assets.h').read_text()
        data=bytes.fromhex(header.split('kJapanese16[] PROGMEM = {')[1].split('};')[0].replace('0x','').replace(',',' '))
        count=struct.unpack_from('<I',data,4)[0]
        points={struct.unpack_from('<I',data,12+i*16)[0] for i in range(count)}
        self.assertTrue(subset.read_extra(subset.CHINESE_FILE)<=points)
        self.assertTrue(subset.korean_points()<=points)
        self.assertTrue(subset.read_extra()<=points)
if __name__=='__main__':unittest.main()
